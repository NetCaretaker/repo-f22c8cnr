#include "lua_exec.hpp"

#include <Windows.h>
#include <string>
#include <cstdint>
#include <cstdio>

#include "MinHook.h"
#include "Main/Scanner.h"
#include "Main/encrypt/skStr.h"


typedef void(*lua_settop_t)(void* L, int idx);
typedef int(*luaL_loadbufferx_t)(void* L, const char* buf, size_t sz, const char* name);
typedef int(*lua_pcallk_t)(void* L, int nargs, int nresults, int errfunc, int ctx);
typedef const char* (*lua_tolstring_t)(void* L, int idx, size_t* len);
typedef void* (*lua_tothread_t)(void* L, int idx);
typedef int(*lua_CFunction)(void* L);
typedef void(*lua_pushcfunction_t)(void* L, lua_CFunction fn);
typedef void(*lua_pushstring_t)(void* L, const char* s);
typedef void(*lua_pushvalue_t)(void* L, int idx);
typedef int(*lua_type_t)(void* L, int idx);
typedef void(*lua_getfield_t)(void* L, int idx, const char* k);
typedef void(*lua_createtable_t)(void* L, int narr, int nrec);
typedef void(*lua_setfield_t)(void* L, int idx, const char* k);
typedef void(*lua_rawseti_t)(void* L, int idx, int n);
typedef int(*lua_rawgeti_t)(void* L, int idx, int n);
typedef void*(*lua_newuserdatauv_t)(void* L, size_t sz, int nuvalue);
typedef void(*lua_pushinteger_t)(void* L, intptr_t n);


// COM-style function typedefs
typedef uintptr_t(__stdcall* QueryInterface_t)(void*, const GUID*, void**);
typedef uintptr_t(__stdcall* AddRef_t)(void*);
typedef uintptr_t(__stdcall* Release_t)(void*);
typedef uintptr_t(__stdcall* PushRuntime_t)(void*, void*);
typedef uintptr_t(__stdcall* PopRuntime_t)(void*, void*);


static uint64_t g_LuaBase = 0;
static uint64_t g_CoreBase = 0;
static uint64_t g_RunFileInternalAddr = 0;
static uint64_t g_LuaL_loadbufferxAddr = 0;
static uint64_t g_Lua_pcallkAddr = 0;
static uint64_t g_Lua_settopAddr = 0;
static uint64_t g_Lua_tolstringAddr = 0;

// IScriptRuntimeHandler* obtained from citizen-scripting-core.dll
static void* g_ScriptRuntimeHandler = nullptr;
static PushRuntime_t g_PushRuntimeFn = nullptr;
static PopRuntime_t g_PopRuntimeFn = nullptr;

// Captured LuaScriptRuntime* and lua_State*
static void* g_LuaRuntime = nullptr;
static void* g_LuaState = nullptr;

// Detected offset: lua_State* inside LuaScriptRuntime
static int g_LuaStateOffset = 0;

// Hook trampoline (original RunFileInternal)
static void* g_OriginalRunFileInternal = nullptr;

// Thread safety
static CRITICAL_SECTION g_LuaCS;


static const uint8_t GUID_SCRIPTRUNTIMEHANDLER_BIN[16] = {
    0x94, 0x71, 0x1E, 0xC4, 0x56, 0x75, 0x02, 0x4C,
    0xBA, 0x45, 0xA9, 0xC8, 0x4D, 0x18, 0xAD, 0x43
};


static constexpr const char* PATTERN_HANDLER =
    "48 8D 0D ? ? ? ? 48 8D 15 ? ? ? ? 48 8D 4C 24 ? E8";


static std::vector<int> PatternToBytes(const char* pattern)
{
    std::vector<int> bytes;
    const char* p = pattern;
    while (*p)
    {
        if (*p == ' ' || *p == '\t') { ++p; continue; }
        if (*p == '?')
        {
            bytes.push_back(-1);
            ++p;
            if (*p == '?') ++p;
        }
        else if (std::isxdigit(static_cast<unsigned char>(*p)))
        {
            bytes.push_back(static_cast<int>(std::strtol(p, const_cast<char**>(&p), 16)));
        }
        else { ++p; }
    }
    return bytes;
}

static uintptr_t FindPatternFull(HMODULE hMod, const char* pattern)
{
    auto bytes = PatternToBytes(pattern);
    if (bytes.empty()) return 0;

    const uintptr_t base = reinterpret_cast<uintptr_t>(hMod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);

    // Scan only the code section (.text)
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        if (memcmp(sec[i].Name, ".text", 5) == 0 ||
            (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE))
        {
            const size_t sz = sec[i].Misc.VirtualSize;
            const uintptr_t start = base + sec[i].VirtualAddress;

            for (size_t j = 0; j < sz - bytes.size(); j++)
            {
                bool found = true;
                for (size_t k = 0; k < bytes.size(); k++)
                {
                    if (bytes[k] != -1 &&
                        reinterpret_cast<const uint8_t*>(start)[j + k] != static_cast<uint8_t>(bytes[k]))
                    {
                        found = false;
                        break;
                    }
                }
                if (found)
                    return start + j;
            }
        }
    }
    return 0;
}

static uintptr_t ResolveRelativeAddress(uintptr_t addr, size_t offsetIntoInst)
{
    int32_t disp = *reinterpret_cast<int32_t*>(addr + offsetIntoInst);
    return addr + offsetIntoInst + 4 + disp;
}


static bool ResolveRuntimeHandler()
{
    if (g_ScriptRuntimeHandler)
        return true;

    HMODULE hCore = GetModuleHandleA(sk("citizen-scripting-core.dll").decrypt());
    if (!hCore)
        return false;

    g_CoreBase = (uint64_t)hCore;

    const uintptr_t base = (uintptr_t)hCore;
    const auto* dos = (const IMAGE_DOS_HEADER*)base;
    const auto* nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);

    uintptr_t guidAddr = 0;
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        if (sec[i].VirtualAddress > 0)
        {
            size_t sz = sec[i].Misc.VirtualSize;
            uintptr_t start = base + sec[i].VirtualAddress;

            for (size_t j = 0; j < sz - sizeof(GUID_SCRIPTRUNTIMEHANDLER_BIN); j++)
            {
                if (memcmp((void*)(start + j), GUID_SCRIPTRUNTIMEHANDLER_BIN, 16) == 0)
                {
                    guidAddr = start + j;
                    break;
                }
            }
            if (guidAddr) break;
        }
    }

    if (!guidAddr)
    {
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
        {
            size_t sz = sec[i].Misc.VirtualSize;
            uintptr_t start = base + sec[i].VirtualAddress;

            for (size_t j = 0; j < sz - sizeof(GUID_SCRIPTRUNTIMEHANDLER_BIN); j++)
            {
                if (memcmp((void*)(start + j), GUID_SCRIPTRUNTIMEHANDLER_BIN, 16) == 0)
                {
                    guidAddr = start + j;
                    break;
                }
            }
            if (guidAddr) break;
        }
    }

    if (!guidAddr)
        return false;

    const uint8_t lea_pattern[] = { 0x48, 0x8D, 0x0D };
    const uint8_t lea2_pattern[] = { 0x48, 0x8D, 0x15 }; // lea rdx,...

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE))
            continue;

        size_t sz = sec[i].Misc.VirtualSize;
        uintptr_t start = base + sec[i].VirtualAddress;

        for (size_t j = 0; j < sz - 7; j++)
        {
            const uint8_t* p = (const uint8_t*)(start + j);

            // Check for: lea rcx, [rip + disp]
            if (memcmp(p, lea_pattern, 3) == 0)
            {
                int32_t disp = *(int32_t*)(p + 3);
                uintptr_t target = start + j + 7 + disp; // RIP = start+j+7

                if (target == guidAddr)
                {

                    uintptr_t funcStart = start + j;
                    while (funcStart > start + 1)
                    {
                        funcStart--;
                        if (funcStart < start) break;
                        if (*(uint8_t*)funcStart == 0xCC) { funcStart++; break; }
                        // retn  →  C3
                        if (*(uint8_t*)funcStart == 0xC3) { funcStart++; break; }
                        // int3  →  CC
                        if (*(uint8_t*)funcStart == 0xCC) { funcStart++; break; }
                    }

                    for (size_t k = 0; k < (j + 20); k++)
                    {
                        const uint8_t* q = (const uint8_t*)(funcStart + k);

                        // mov [rip+disp32], rax  →  48 89 05
                        if (q[0] == 0x48 && q[1] == 0x89 && q[2] == 0x05)
                        {
                            int32_t d = *(int32_t*)(q + 3);
                            uintptr_t varAddr = funcStart + k + 7 + d;

                            // Sanity check: target should be in .data/.rdata
                            for (WORD si = 0; si < nt->FileHeader.NumberOfSections; si++)
                            {
                                uintptr_t sStart = base + sec[si].VirtualAddress;
                                if (varAddr >= sStart && varAddr < sStart + sec[si].Misc.VirtualSize)
                                {
                                    // Read the handler pointer
                                    void** ppHandler = (void**)varAddr;
                                    if (ppHandler && *ppHandler)
                                    {
                                        // Verify it looks like a COM object (vtable pointer)
                                        void* vtable = *(void**)ppHandler;
                                        MEMORY_BASIC_INFORMATION mbi = {};
                                        if (VirtualQuery(vtable, &mbi, sizeof(mbi)) &&
                                            mbi.State == MEM_COMMIT &&
                                            (mbi.Protect & PAGE_READONLY))
                                        {
                                            g_ScriptRuntimeHandler = *ppHandler;
                                            if (g_ScriptRuntimeHandler)
                                            {
                                                // Resolve PushRuntime from vtable[3]
                                                void** vtbl = *(void***)g_ScriptRuntimeHandler;
                                                g_PushRuntimeFn = (PushRuntime_t)vtbl[3];
                                                g_PopRuntimeFn = (PopRuntime_t)vtbl[5];
                                                return true;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // Try: mov rax, qword ptr [static_addr] → 48 8B 05
                        if (q[0] == 0x48 && q[1] == 0x8B && q[2] == 0x05)
                        {
                            int32_t d = *(int32_t*)(q + 3);
                            uintptr_t varAddr = funcStart + k + 7 + d;

                            void** ppHandler = (void**)varAddr;
                            if (ppHandler && *ppHandler)
                            {
                                void* vtable = *(void**)ppHandler;
                                MEMORY_BASIC_INFORMATION mbi = {};
                                if (VirtualQuery(vtable, &mbi, sizeof(mbi)) &&
                                    mbi.State == MEM_COMMIT &&
                                    (mbi.Protect & PAGE_READONLY))
                                {
                                    g_ScriptRuntimeHandler = *ppHandler;
                                    if (g_ScriptRuntimeHandler)
                                    {
                                        void** vtbl = *(void***)g_ScriptRuntimeHandler;
                                        g_PushRuntimeFn = (PushRuntime_t)vtbl[3];
                                        g_PopRuntimeFn = (PopRuntime_t)vtbl[5];
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}


static int DetectLuaStateOffset(uint64_t funcAddr)
{
    if (!funcAddr) return 0;

    for (int i = 0; i < 60; i++)
    {
        uint8_t* p = (uint8_t*)(funcAddr + i);

        // mov rcx, [rcx + disp8]  48 8B 49 XX
        if (p[0] == 0x48 && p[1] == 0x8B && p[2] == 0x49)
            return p[3];

        // mov rdi, [rcx + disp8]  48 8B 79 XX
        if (p[0] == 0x48 && p[1] == 0x8B && p[2] == 0x79)
            return p[3];

        // mov rsi, [rcx + disp8]  48 8B 71 XX
        if (p[0] == 0x48 && p[1] == 0x8B && p[2] == 0x71)
            return p[3];

        // mov rcx, [rax + disp8]  48 8B 48 XX
        if (p[0] == 0x48 && p[1] == 0x8B && p[2] == 0x48)
            return p[3];

        // mov rax, [rcx + disp8]  48 8B 41 XX
        if (p[0] == 0x48 && p[1] == 0x8B && p[2] == 0x41)
            return p[3];
    }

    return 0;
}


static bool PushLuaEnvironment(void* runtime)
{
    if (!g_ScriptRuntimeHandler || !g_PushRuntimeFn || !runtime)
        return false;

    g_PushRuntimeFn(g_ScriptRuntimeHandler, runtime);
    return true;
}

static void PopLuaEnvironment(void* runtime)
{
    if (g_ScriptRuntimeHandler && g_PopRuntimeFn && runtime)
    {
        g_PopRuntimeFn(g_ScriptRuntimeHandler, runtime);
    }
}


static void __fastcall RunFileInternalDetour(void* self, void* _rdx, void* _r8, void* _r9)
{
    if (self)
    {
        g_LuaRuntime = self;

        if (g_LuaStateOffset > 0)
        {
            void* L = *(void**)((uintptr_t)self + g_LuaStateOffset);
            if (L && L != g_LuaState)
            {
                // Sanity check: readable pointer
                MEMORY_BASIC_INFORMATION mbi = {};
                if (VirtualQuery(L, &mbi, sizeof(mbi)) &&
                    mbi.State == MEM_COMMIT &&
                    (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
                {
                    g_LuaState = L;
                }
            }
        }
    }

    typedef void(__fastcall* OrigFunc)(void*, void*, void*, void*);
    ((OrigFunc)g_OriginalRunFileInternal)(self, _rdx, _r8, _r9);
}

void InitLuaExec()
{
    if (g_LuaBase)
        return;

    InitializeCriticalSection(&g_LuaCS);

    //  Find citizen-scripting-lua.dll 
    g_LuaBase = (uint64_t)GetModuleHandleA(sk("citizen-scripting-lua.dll").decrypt());
    if (!g_LuaBase)
        return;

    //  Scan for required functions 
    static bool s_scanned = false;
    if (!s_scanned)
    {
        const auto modName = sk("citizen-scripting-lua.dll").decrypt();
        g_RunFileInternalAddr = Scanner::Get()->Scan(
            modName,
            "48 8B C4 55 56 57 41 54 41 55 41 56 41 57 48 8D A8",
            NULL, 0
        );
        g_LuaL_loadbufferxAddr = Scanner::Get()->Scan(
            modName,
            "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 54 41 55 41 56 41 57 48 83 EC ? 49 8B D8",
            NULL, 0
        );
        g_Lua_pcallkAddr = Scanner::Get()->Scan(
            modName,
            "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 54 41 55 41 56 41 57 48 83 EC ? 44 8B F2",
            NULL, 0
        );
        g_Lua_settopAddr = Scanner::Get()->Scan(
            modName,
            "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC ? 48 8B E9",
            NULL, 0
        );
        g_Lua_tolstringAddr = Scanner::Get()->Scan(
            modName,
            "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 63 FA 49 8B F0",
            NULL, 0
        );
        s_scanned = true;
    }

    if (!g_RunFileInternalAddr || !g_LuaL_loadbufferxAddr || !g_Lua_pcallkAddr)
    {
        g_LuaBase = 0;
        return;
    }

    //  Detect lua_State offset 
    g_LuaStateOffset = DetectLuaStateOffset(g_RunFileInternalAddr);
    if (g_LuaStateOffset == 0)
    {
        g_LuaBase = 0;
        return;
    }

    //  Resolve IScriptRuntimeHandler for PushEnvironment 
    ResolveRuntimeHandler();

    //  Install MinHook on RunFileInternal 
    if (MH_Initialize() != MH_OK)
    {
        // Already initialised  not fatal
    }

    if (MH_CreateHook(
            (void*)g_RunFileInternalAddr,
            &RunFileInternalDetour,
            &g_OriginalRunFileInternal) == MH_OK)
    {
        MH_EnableHook((void*)g_RunFileInternalAddr);
    }
    else
    {
        g_LuaBase = 0;
    }
}


bool DirectLuaExec(const std::string& script)
{
    if (!g_LuaState || !g_LuaL_loadbufferxAddr || !g_Lua_pcallkAddr)
        return false;

    // Resolve Lua API functions (one-time)
    static auto fn_luaL_loadbufferx = (luaL_loadbufferx_t)g_LuaL_loadbufferxAddr;
    static auto fn_lua_pcallk       = (lua_pcallk_t)g_Lua_pcallkAddr;
    static auto fn_lua_settop       = (lua_settop_t)g_Lua_settopAddr;
    static auto fn_lua_tolstring    = (lua_tolstring_t)g_Lua_tolstringAddr;

    EnterCriticalSection(&g_LuaCS);

    //  Push the runtime environment if available 
    PushLuaEnvironment(g_LuaRuntime);

    //  Wrap script in protected call with error printing 
    std::string wrapped =
        "local ok,err=pcall(function() " + script + " end) "
        "if not ok then print('[Pink] '..tostring(err)) end";

    bool success = false;

    //  Compile 
    int loadResult = fn_luaL_loadbufferx(g_LuaState, wrapped.c_str(), wrapped.size(), sk("[Pink]").decrypt());
    if (loadResult != 0)
    {
        if (fn_lua_tolstring && fn_lua_settop)
        {
            const char* errStr = fn_lua_tolstring(g_LuaState, -1, nullptr);
            fn_lua_settop(g_LuaState, -1);
            if (errStr)
            {
                // Use the overloaded print function
                std::string sysCmd = "print('[Pink] Compile error: ' .. " +
                    std::string(errStr) + ")";
                // Can't execute this easily, but the error was on the stack
            }
        }
        goto cleanup;
    }

    //  Execute 
    {
        int callResult = fn_lua_pcallk(g_LuaState, 0, 0, 0, 0);
        if (callResult != 0)
        {
            if (fn_lua_tolstring && fn_lua_settop)
            {
                const char* errStr = fn_lua_tolstring(g_LuaState, -1, nullptr);
                fn_lua_settop(g_LuaState, -1);
                (void)errStr;
            }
            goto cleanup;
        }
    }

    success = true;

cleanup:
    //  Pop the runtime environment 
    PopLuaEnvironment(g_LuaRuntime);

    LeaveCriticalSection(&g_LuaCS);
    return success;
}


bool DirectLuaExecRaw(const std::string& script)
{
    if (!g_LuaState || !g_LuaL_loadbufferxAddr || !g_Lua_pcallkAddr)
        return false;

    static auto fn_luaL_loadbufferx = (luaL_loadbufferx_t)g_LuaL_loadbufferxAddr;
    static auto fn_lua_pcallk       = (lua_pcallk_t)g_Lua_pcallkAddr;
    static auto fn_lua_settop       = (lua_settop_t)g_Lua_settopAddr;

    EnterCriticalSection(&g_LuaCS);

    PushLuaEnvironment(g_LuaRuntime);

    bool success = false;

    // Load script directly without any wrapper
    {
        auto chunkName = sk("[Pink-Raw]");
        int loadResult = fn_luaL_loadbufferx(g_LuaState, script.c_str(), script.size(), chunkName.decrypt());
        if (loadResult != 0)
        {
            fn_lua_settop(g_LuaState, -1);
            PopLuaEnvironment(g_LuaRuntime);
            LeaveCriticalSection(&g_LuaCS);
            return false;
        }
    }

    // Execute
    {
        int callResult = fn_lua_pcallk(g_LuaState, 0, 0, 0, 0);
        if (callResult != 0)
        {
            fn_lua_settop(g_LuaState, -1);
            PopLuaEnvironment(g_LuaRuntime);
            LeaveCriticalSection(&g_LuaCS);
            return false;
        }
    }

    success = true;

    PopLuaEnvironment(g_LuaRuntime);
    LeaveCriticalSection(&g_LuaCS);
    return success;
}

bool DirectLuaExecEx(const std::string& script, bool asThread)
{
    if (!asThread)
        return DirectLuaExec(script);

    if (!g_LuaState || !g_LuaL_loadbufferxAddr || !g_Lua_pcallkAddr)
        return false;

    static auto fn_luaL_loadbufferx = (luaL_loadbufferx_t)g_LuaL_loadbufferxAddr;
    static auto fn_lua_pcallk       = (lua_pcallk_t)g_Lua_pcallkAddr;
    static auto fn_lua_settop       = (lua_settop_t)g_Lua_settopAddr;
    static auto fn_lua_tolstring    = (lua_tolstring_t)g_Lua_tolstringAddr;

    EnterCriticalSection(&g_LuaCS);

    // Push the runtime environment
    bool envPushed = PushLuaEnvironment(g_LuaRuntime);


    std::string wrapped =
        "Citizen.CreateThreadNow(function() "
        "  local ok,err=pcall(function() " + script + " end) "
        "  if not ok then print('[Pink] '..tostring(err)) end "
        "end, '[Pink]')";

    bool success = false;

    int loadResult = fn_luaL_loadbufferx(g_LuaState, wrapped.c_str(), wrapped.size(), sk("[Pink]").decrypt());
    if (loadResult != 0)
    {
        fn_lua_settop(g_LuaState, -1);
        goto cleanupEx;
    }

    {
        int callResult = fn_lua_pcallk(g_LuaState, 0, 0, 0, 0);
        if (callResult != 0)
        {
            fn_lua_settop(g_LuaState, -1);
            goto cleanupEx;
        }
    }

    success = true;

cleanupEx:
    if (envPushed)
        PopLuaEnvironment(g_LuaRuntime);

    LeaveCriticalSection(&g_LuaCS);
    return success;
}

bool IsLuaReady()
{
    return (g_LuaState != nullptr);
}

bool IsLuaExecReady()
{
    return (g_LuaState != nullptr) && (g_LuaRuntime != nullptr);
}
