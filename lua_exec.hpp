#pragma once
#include <string>

// ── Initialisation ──
// Call once after Address::Get()->Load() and when citizen-scripting-lua.dll is loaded.
void InitLuaExec();

// ── Basic Lua execution ──
// Executes a Lua script string on the captured FiveM Lua state.
// Wraps the script in pcall() for error handling.
// Prefer this for simple scripts (math, strings, basic natives).
bool DirectLuaExec(const std::string& script);

// ── Raw Lua execution ──
// Executes a Lua script WITHOUT any wrapper (no pcall, no function wrap).
// Use for obfuscated/encrypted scripts that set _ENV or need top-level execution.
bool DirectLuaExecRaw(const std::string& script);

// ── Scheduler-aware Lua execution ──
// If asThread=true: wraps in Citizen.CreateThreadNow for full scheduler integration.
// Enables: Wait(), events, exports, promises.
// Requires IScriptRuntimeHandler to be resolved.
bool DirectLuaExecEx(const std::string& script, bool asThread = true);

// ── Status checks ──
bool IsLuaReady();       // true once lua_State* is captured
bool IsLuaExecReady();   // true once both lua_State* and runtime are captured
