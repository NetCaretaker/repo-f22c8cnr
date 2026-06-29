#include "address.h"
#include "Psapi.h"
#include <TlHelp32.h>
#include "../../Includes.h"
#include "../../scroupthick.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <shlobj.h>
Address* Address::s_pSingleton = nullptr;

void CopyToClipboard(const std::string& text) {
	if (OpenClipboard(nullptr)) {
		EmptyClipboard();
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
		if (hMem) {
			memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
			GlobalUnlock(hMem);
			SetClipboardData(CF_TEXT, hMem);
		}
		CloseClipboard();
	}
}
DWORD_PTR get_proc_base(DWORD procId) {
	DWORD_PTR baseaddress = 0;
	HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
	HMODULE* moduleArray;
	LPBYTE moduleArrayBytes;
	DWORD bytesRequired;
	if (processHandle)
	{
		if (K32EnumProcessModules(processHandle, NULL, 0, &bytesRequired))
		{
			if (bytesRequired)
			{
				moduleArrayBytes = (LPBYTE)LocalAlloc(LPTR, bytesRequired);
				if (moduleArrayBytes)
				{
					unsigned int moduleCount;
					moduleCount = bytesRequired / sizeof(HMODULE);
					moduleArray = (HMODULE*)moduleArrayBytes;
					if (EnumProcessModules(processHandle, moduleArray, bytesRequired, &bytesRequired))
					{
						baseaddress = (DWORD_PTR)moduleArray[0];
					}
					LocalFree(moduleArrayBytes);
				}
			}
		}
		CloseHandle(processHandle);
	}
	return baseaddress;
}
int get_build() {
	
	char exePath[MAX_PATH] = { 0 };
	DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
	if (len > 0) {
		std::string exeName(exePath);
		size_t pos = exeName.find_last_of("\\/");
		if (pos != std::string::npos) exeName = exeName.substr(pos + 1);
		size_t bPos = exeName.find("_b");
		if (bPos != std::string::npos) {
			try { return std::stoi(exeName.substr(bPos + 2)); } catch (...) {}
		}
	}

	
	static const std::unordered_map<std::string, int> buildMap = {
		{"Dec  5 2024", 3407},
		{"Sep 10 2024", 3323},
		{"Jun 20 2024", 3258},
		{"Dec  8 2023", 3095},
		{"Jun  8 2023", 2944},
		{"Dec  8 2022", 2802},
		{"Jul 21 2022", 2699},
		{"Apr 18 2022", 2612},
		{"Dec 14 2021", 2545},
		{"Jul 15 2021", 2372},
		{"Dec 10 2020", 2189},
		{"Aug  5 2020", 2060},
		{"Dec 11 2019", 1868},
		{"Jul 23 2019", 1737},
		{"Dec  5 2018", 1604},
		{"Oct 14 2018", 1493},
		{"Mar 14 2018", 1365},
		{"Dec  6 2017", 1290},
		{"Jun  9 2017", 1103},
		{"Mar 31 2017", 1032},
		{"Oct 13", 505},
		{"Jun 30", 393},
		{"Jun  9 2015", 372}
	};

	DWORD_PTR baseAddress = get_proc_base(GetCurrentProcessId());
	uintptr_t offset = (uintptr_t)(baseAddress + 0x13ee1a);
	if (offset) {
		char* buildStringPtr = (char*)(offset + *(int32_t*)offset + 4);
		std::string buildString(buildStringPtr);
		auto it = buildMap.find(buildString);
		if (it != buildMap.end()) return it->second;
	}
	return -1;
}

static void InitializeRaycast(uint64_t image_base, int gameNumber) {
	uintptr_t raycastOffset = 0;
	switch (gameNumber) {
		case 3407:
			raycastOffset = 0xD8AEA8;
			break;
		default:
			raycastOffset = 0xD75E9C;
			break;
	}
	
	__try {
		uintptr_t raycastPtrAddr = image_base + raycastOffset;
		uintptr_t raycastFuncPtr = *(uintptr_t*)raycastPtrAddr;
		if (raycastFuncPtr) {
			MemoryAddress::raycast = reinterpret_cast<raycast_t>(raycastFuncPtr);
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {
		MemoryAddress::raycast = nullptr;
	}
}

void Address::Load() {
	MemoryAddress::explose_vehicule = reinterpret_cast<explode_vehicule>(Scanner::Get()->Scan(NULL, sk("48 8B 41 ? 83 78 ? ? 8B 08 0F 95 C2 83 78 ? ? 41 0F 95 C0 E9 ? ? ? ? CC CC 48 8B 41 ? 83 78 ? ? 8B 08 0F 95 C2 E9 ? ? ? ? CC CC 40 53"), NULL, NULL));
	

	MemoryAddress::s_OrigAccurate = Scanner::Get()->Scan(NULL, sk("48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 48 89 78 ? 41 56 48 83 EC 70 0F 29 70 ? 48 8B F1"), NULL, NULL);
	MemoryAddress::add_weapon = reinterpret_cast<add_weapon_t>(Scanner::Get()->Scan(NULL, sk("48 89 5c 24 ? 48 89 6c 24 ? 48 89 74 24 ? 57 48 83 ec ? 41 8b f0 8b fa 48 8b d9 e8"), NULL, NULL));
	
	auto scanproot = Scanner::Get()->Scan(NULL, sk("4C 8B 25 ? ? ? ? 8B 29 33 F6 49 8B 04 24 33 DB 4C 8D 71 08 44 8B 78 08 45 85 FF 0F 8E ? ? ? ? 4D 8B 0C 24 41 3B 59 08 7D 29 49 8B 51 30 44 8B C3 8B CB 49 C1 E8 05 83 E1 1F 44 8B D3 42 8B 04 82"), NULL, NULL);
	uint64_t offsett = 3;
	if (scanproot) {
		uint64_t rip_offset = *(int32_t*)(scanproot + offsett);
		uint64_t absolute_address = scanproot + offsett + 4 + rip_offset;
		s_mVehiclePool = (VehiclePool***)(absolute_address);
	}
	else {
		s_mVehiclePool = nullptr;
	}

	
	auto scanprooot = Scanner::Get()->Scan(NULL, sk("4C 8B 05 ?? ?? ?? ?? 0F 29 78 D8"), NULL, NULL);
	if (scanprooot) {
		uint64_t ripp_offset = *(int32_t*)(scanprooot + offsett);
		uint64_t absolutee_address = scanprooot + offsett + 4 + ripp_offset;
		s_mPedPool = (GenericPool**)(absolutee_address);
	}
	else {
		s_mPedPool = nullptr;
	}
	
	dwThreadCollectionPtr = Scanner::Get()->Scan(NULL, sk("63 6F 6D 6D 6F 6E 3A 2F 64 61 74 61 2F 73 63 72 69 70 74 4D 65 74 61 64 61 74 61"), NULL, NULL);
	
	auto tlsScan = Scanner::Get()->Scan(NULL, sk("48 8B 04 D0 4A 8B 14 00 48 8B 01 F3 44 0F 2C 42 20"), NULL, NULL);
	
	if (tlsScan) activeThreadTlsOffset = *(uint32_t*)(tlsScan - 4); else activeThreadTlsOffset = 0;
	
	auto tickScan = Scanner::Get()->Scan(NULL, sk("80 B9 ? 01 00 00 00 8B FA 48 8B D9 74 05"), NULL, NULL);
	
	if (tickScan) {
		auto tickFunc = tickScan - 0xF;
		tickFuncPtr = Scanner::Get()->resolvePtr(0, tickFunc, 0xE9);
	}
	else {
		tickFuncPtr = 0;
	}

	
	MemoryAddress::request_model = reinterpret_cast<request_model_t>(Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 48 89 7C 24 ? 55 48 8B EC 48 83 EC 50 8B 45"), NULL, NULL));
	MemoryAddress::set_model_no_longer_needed = reinterpret_cast<set_model_no_longer_needed_t>(Scanner::Get()->Scan(NULL, sk("40 53 48 83 EC 30 48 8D 54 24 ? 8B D9"), NULL, NULL));
	MemoryAddress::has_model_loaded = reinterpret_cast<has_model_loaded_t>(Scanner::Get()->Scan(NULL, sk("48 89 7C 24 ? 55 48 8B EC 48 83 EC ? 8B 45 ? BF"), NULL, NULL));
	MemoryAddress::is_model_in_cd_image = reinterpret_cast<is_model_in_cd_image_t>(Scanner::Get()->Scan(NULL, sk("48 89 7C 24 ? 55 48 8B EC 48 83 EC 20 8B 45 ? BF FF FF 00 00"), NULL, NULL));
	MemoryAddress::create_vehicle = reinterpret_cast<create_vehicle_t>(Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 50 F3 0F 10 02"), NULL, NULL));
	MemoryAddress::pointer_to_handle = reinterpret_cast<pizza_to_spaghetti_t>(Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 8B 15 ? ? ? ? 48 8B F9 48 83 C1 10 33 DB"), NULL, NULL));
	MemoryAddress::repair_vehicle = reinterpret_cast<repair_vehicle_t>(Scanner::Get()->Scan(NULL, sk("40 53 48 83 EC 20 E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 4C 8B 10"), NULL, NULL));
	MemoryAddress::set_plate = reinterpret_cast<set_plate_t>(Scanner::Get()->Scan(NULL, sk("40 53 48 83 EC 20 48 8B DA E8 ? ? ? ? 48 85 C0 74 ? 48 8B 48 ? 48 8B D3"), NULL, NULL));
	MemoryAddress::Set_forward_speed = reinterpret_cast<Set_forward_speed_t>(Scanner::Get()->Scan(NULL, sk("48 83 EC 38 0F 29 74 24 ? 0F 28 F1 E8 ? ? ? ? 48 85 C0 74 ? F6 40"), NULL, NULL));
	MemoryAddress::SetEntityCoordsNoOffset = reinterpret_cast<SetEntityCoordsNoOffset_t>(Scanner::Get()->Scan(NULL, sk("48 8B C4 48 89 58 08 48 89 68 10 48 89 70 18 57 41 54 41 55 41 56 41 57 48 81 EC 90 00 00 00 8B"), NULL, NULL));
	MemoryAddress::random_outfit2 = reinterpret_cast<random_outfit2_t>(Scanner::Get()->Scan(NULL, sk("40 53 48 83 EC 60 E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 83 64 24"), NULL, NULL));
	MemoryAddress::random_outfit = reinterpret_cast<random_outfit_t>(Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 57 48 83 EC 40 8B FA E8 ? ? ? ? 48 8B D8"), NULL, NULL));
	MemoryAddress::set_ped_random_component = reinterpret_cast<set_ped_random_component_t>(Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 57 48 83 EC 40 8B FA E8 ? ? ? ? 48 8B D8"), NULL, NULL));
	MemoryAddress::set_ped_random_props = reinterpret_cast<set_ped_random_props_t>(Scanner::Get()->Scan(NULL, sk("40 53 48 83 EC 60 E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 83 64 24"), NULL, NULL));
	
	MemoryAddress::spectate = reinterpret_cast<spectate_t>(Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 57 48 83 EC 20 41 8A F8 84 C9"), NULL, NULL));
	
	MemoryAddress::add_explosion = reinterpret_cast<add_explosion_t>(Scanner::Get()->Scan(NULL, sk("E9 ? ? ? ? 8B 85 ? ? ? ? A8 40 48 8D 64 24 ? 48 89 2C 24 48 BD ? ? ? ? ? ? ? ? 48 87 2C 24 48 89 4C 24"), NULL, NULL));
	
	uintptr_t task_shoot_pattern_result = Scanner::Get()->Scan(NULL, sk("48 8B C4 48 89 58 ? 48 89 70 ? 48 89 78 ? 55 41 56 41 57 48 8D 68 ? 48 81 EC ? ? ? ? 33 FF 45 8B F9"), NULL, NULL);
	MemoryAddress::task_shoot_gun_at_coord = reinterpret_cast<task_shoot_gun_at_coord_t>(task_shoot_pattern_result);
	
	uintptr_t task_shoot_at_entity_pattern_result = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 33 FF 41 8B F1"), NULL, NULL);
	MemoryAddress::task_shoot_at_entity = reinterpret_cast<task_shoot_at_entity_t>(task_shoot_at_entity_pattern_result);

	
	s_pSwapChain = Scanner::Get()->Scan(NULL, sk("48 8B 0D ? ? ? ? 48 8B 01 44 8D 43 01 33 D2 FF 50 40 8B C8"), NULL, 7);
	MemoryAddress::oGameThread_addr = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ?? 57 48 83 EC 20 48 8B D9 E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ?? 8B 83 ?? ?? ?? ??"), NULL, 0);
	
	
	{
		auto __replay = Scanner::Get()->Scan(NULL, sk("48 8D 0D ?? ?? ?? ?? 48 8B D7 E8 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? 8A D8 E8 ?? ?? ?? ?? 84 DB 75 13 48 8D 0D ?? ?? ?? ?? 48 8B D7 E8 ?? ?? ?? ?? 84 C0 74 BC 8B 8F"), NULL, 7);
		(void)__replay;
	}
	
	{
		uint64_t altCam = Scanner::Get()->Scan(NULL, sk("48 8B 05 ? ? ? ? 48 8B 98 ? ? ? ? EB"), NULL, 7);
		if (altCam) s_pViewAngles = altCam;
	}
	MemoryAddress::NetworkRequestControlOfEntity_addr = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ?? 57 48 83 EC 20 8B D9 E8 ?? ?? ?? ?? 84 C0"), NULL, 0);
	MemoryAddress::g_world2screen = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ?? 55 56 57 48 83 EC 70 65 4C 8B 0C"), NULL, 0);
	MemoryAddress::g_bonemask = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 48 83 EC 60 48 8B 01 41 8B E8 48 8B F2 48 8B F9 33 DB"), NULL, 0);
	MemoryAddress::ClearPedTask_addr = Scanner::Get()->Scan(NULL, sk("40 53 48 83 EC 30 E8 ?? ?? ?? ?? 48 8B D8 48 85 C0 0F 84"), NULL, 0);
	MemoryAddress::SetPedAmmo_addr = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ?? 48 89 74 24 ?? 57 48 83 EC 20 41 8B F0 8B FA E8 ?? ?? ?? ?? 48 8B D8"), NULL, 0);
	MemoryAddress::GET_ENTITY_BONE_INDEX_BY_NAME_addr = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 10 48 89 74 24 18 57 48 83 EC ? 48 8B 01 83 4C 24 30 ?"), NULL, 0);
	MemoryAddress::GET_WORLD_POSITION_OF_ENTITY_BONE_addr = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC ? 4C 8B 01 0F 29 74 24 20"), NULL, 0);
	MemoryAddress::give_weapon_to_ped_addr = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 30 41 8A E9 41 8B F0 8B FA E8 ? ? ? ?"), NULL, 0);
	MemoryAddress::no_reload_addr = Scanner::Get()->Scan(NULL, sk("41 2B C9 3B C8 0F"), NULL, 0);
	{
		uint64_t WaypointPatt = Scanner::Get()->Scan(NULL, sk("74 1F F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 03"), NULL, 0);
		if (WaypointPatt) MemoryAddress::WayPointRead = WaypointPatt + *(DWORD*)(WaypointPatt + 6) + 0xA;
	}
	MemoryAddress::ClearPedBlood_addr = Scanner::Get()->Scan(NULL, sk("40 53 48 83 EC 20 8A 91 ? ? ? ? 48 8B D9 80 FA FF 74 51"), NULL, 0);

	
	MemoryAddress::MagicBulletsPatch = Scanner::Get()->Scan(NULL, sk("0F 29 4F ? 83 8F ? ? ? ? ? 48 8B 4F"), NULL, 0);
	MemoryAddress::ArmsKinematics    = Scanner::Get()->Scan(NULL, sk("E8 ? ? ? ? 48 83 C3 60 48 FF CF 75 E6 48 8B 5C 24"), NULL, 0);
	MemoryAddress::LegsKinematics    = Scanner::Get()->Scan(NULL, sk("E8 ? ? ? ? 48 83 C3 60 48 FF CF 75 DF 48 8B 5C 24 ? 48 8B 6C 24 ? 48 8B 74 24"), NULL, 0);
	MemoryAddress::SilentAim         = Scanner::Get()->Scan(NULL, sk("48 8D 45 ? F3 0F 10 00 F3 0F 10 48 ? F3 0F 11 45"), NULL, 0);
	MemoryAddress::InfiniteCombatRoll= Scanner::Get()->Scan(NULL, sk("89 81 ? ? ? ? 8B 87 ? ? ? ? F7 D0"), NULL, 0);
	MemoryAddress::InfiniteAmmo0     = Scanner::Get()->Scan(NULL, sk("41 2B C9 3B C8 0F 4D C8"), NULL, 0);
	MemoryAddress::InfiniteAmmo1     = Scanner::Get()->Scan(NULL, sk("41 2B D1 E8"), NULL, 0);
	MemoryAddress::AimCPedPatternResult = Scanner::Get()->Scan(NULL, sk("48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ? 48 85 C9 74 05 E8 ? ? ? ? 8A CB"), NULL, 0);
	MemoryAddress::SpectatorBypass   = Scanner::Get()->Scan(NULL, sk("44 8A 05 ? ? ? ? 48 8B CB 8B D0 E8 ? ? ? ? 88 05 ? ? ? ? 48 83 C4 ? 5B C3 90 48 89 5C 24 ? 57"), NULL, 0);
	
	if (MemoryAddress::SpectatorBypass) {
		uintptr_t base = MemoryAddress::SpectatorBypass;
		int32_t loadDisp = *(int32_t*)(base + 3);
		MemoryAddress::SpectatorFlagPtr1 = base + 7 + loadDisp;
		int32_t storeDisp = *(int32_t*)(base + 19);
		MemoryAddress::SpectatorFlagPtr2 = base + 23 + storeDisp;
	}
	
	
	uintptr_t longPat = Scanner::Get()->Scan(NULL, sk("48 89 5C 24 ? 57 48 83 EC 20 8B D9 E8 ? ? ? ? 84 C0"), NULL, 0);
	if (longPat && *(uint8_t*)(longPat + 17) == 0x84 && *(uint8_t*)(longPat + 18) == 0xC0)
		MemoryAddress::SpectatorBypassTestJnz = longPat + 17;
	if (!MemoryAddress::SpectatorBypassTestJnz)
		MemoryAddress::SpectatorBypassTestJnz = Scanner::Get()->Scan(NULL, sk("84 C0 75 6A 8B CB"), NULL, 0);
	if (!MemoryAddress::SpectatorBypassTestJnz)
		MemoryAddress::SpectatorBypassTestJnz = Scanner::Get()->Scan(NULL, sk("84 C0 75 ? 8B CB"), NULL, 0);
	
	if (!MemoryAddress::SpectatorBypassTestJnz) {
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
		if (hSnap != INVALID_HANDLE_VALUE) {
			MODULEENTRY32 me = { 0 };
			me.dwSize = sizeof(me);
			if (Module32First(hSnap, &me)) {
				do {
					uintptr_t base = (uintptr_t)me.modBaseAddr;
					uintptr_t r = Scanner::Get()->ReturnScan(base, sk("48 89 5C 24 ? 57 48 83 EC 20 8B D9 E8 ? ? ? ? 84 C0"), 0);
					if (r && *(uint8_t*)(r + 17) == 0x84 && *(uint8_t*)(r + 18) == 0xC0) {
						MemoryAddress::SpectatorBypassTestJnz = r + 17;
						break;
					}
					r = Scanner::Get()->ReturnScan(base, sk("84 C0 75 6A 8B CB"), 0);
					if (r) { MemoryAddress::SpectatorBypassTestJnz = r; break; }
					r = Scanner::Get()->ReturnScan(base, sk("84 C0 75 ? 8B CB"), 0);
					if (r) { MemoryAddress::SpectatorBypassTestJnz = r; break; }
				} while (Module32Next(hSnap, &me));
			}
			CloseHandle(hSnap);
		}
	}

	int gameNumber = get_build();
	version = gameNumber;
	uint64_t image_base = *(uint64_t*)(__readgsqword(0x60) + 0x10);

	
	InitializeRaycast(image_base, gameNumber);

	
	{
		auto worldPtr = Scanner::Get()->Scan(NULL, sk("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), NULL, 7);
		if (worldPtr) m_sPedFactory = worldPtr;
		auto viewPortPtr = Scanner::Get()->Scan(NULL, sk("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), NULL, 7);
		if (viewPortPtr) s_pViewPort = viewPortPtr;
		auto cameraPtr = Scanner::Get()->Scan(NULL, sk("48 8B 05 ? ? ? ? 48 8B 98 ? ? ? ? EB"), NULL, 7);
		if (cameraPtr) s_pViewAngles = cameraPtr;
		if (!s_pViewAngles) {
			auto cameraAlt = Scanner::Get()->Scan(NULL, sk("48 8B 05 ? ? ? ? 38 98 ? ? ? ? 8A C3"), NULL, 7);
			if (cameraAlt) s_pViewAngles = cameraAlt;
		}
		auto playerNamePtr = Scanner::Get()->Scan(NULL, sk("48 8B 15 ? ? ? ? 48 C1 E1"), NULL, 7);
		if (playerNamePtr) s_pPlayerNamesList = playerNamePtr;
	}

	switch (gameNumber) {
	case 3570:
		if (!s_pPlayerNamesList) s_pPlayerNamesList = (image_base + 0x2F7C648);
		if (!m_sPedFactory) m_sPedFactory = (image_base + 0x25EC580);
		if (!s_pViewPort) s_pViewPort = (image_base + 0x2058BA0);
		if (!s_pViewAngles) s_pViewAngles = (image_base + 0x2059A48);
		if (!s_pSwapChain) s_pSwapChain = (image_base + 0x2D56D08);
		m_pPedTask = 0x144B;
		m_pEntityType = 0x1098;
		m_pArmor = 0x150C;
		m_pWeaponManager = 0x10B8;
		m_pPlayerInfo = 0x10A8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x13C0;
		m_pEnghealth = 0x08E8;
		m_pNetid = 0xE8;
		m_pPlayerName = 0xA4;
		m_pVehMgr = 0x0D10;
		m_pVelocity = 0x300;
		m_pGravity = 0xC8C;
		m_pHandlingData = 0x960;
		m_pFrameFlags = 0x270;
		m_pConfigFlags = 0x1444;
		m_pBoneOffset = 0x410;
		break;
	case 3407:
		if (!s_pPlayerNamesList) s_pPlayerNamesList = (image_base + 0x2F478A8);
		if (!m_sPedFactory) m_sPedFactory = (image_base + 0x25D7108);
		if (!s_pViewPort) s_pViewPort = (image_base + 0x20431C0);
		if (!s_pViewAngles) s_pViewAngles = (image_base + 0x20440C8);
		if (!s_pSwapChain) s_pSwapChain = (image_base + 0x2D22808);
		m_pPedTask = 0x144B;
		m_pEntityType = 0x1098;
		m_pArmor = 0x150C;
		m_pWeaponManager = 0x10B8;
		m_pPlayerInfo = 0x10A8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x13C0;
		m_pEnghealth = 0x08E8;
		m_pNetid = 0xE8;
		m_pPlayerName = 0xA4;
		m_pVehMgr = 0x0D10;
		m_pVelocity = 0x300;
		m_pGravity = 0xC8C;
		m_pHandlingData = 0x960;
		m_pFrameFlags = 0x270;
		m_pConfigFlags = 0x1444;
		m_pBoneOffset = 0x410;
		break;
	case 3323:
		m_pPedTask = 0x144B;
		m_pEntityType = 0x1098;
		m_pArmor = 0x150C;
		m_pWeaponManager = 0x10B8;
		m_pPlayerInfo = 0x10A8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x13C0;
		m_pEnghealth = 0x08E8;
		m_pNetid = 0xE8;
		m_pPlayerName = 0xA4;
		m_pVehMgr = 0x0D10;
		m_pVelocity = 0x300;
		m_pGravity = 0xC8C;
		m_pHandlingData = 0x960;
		m_pFrameFlags = 0x270;
		m_pConfigFlags = 0x1444;
		m_pBoneOffset = 0x410;
		break;
	case 3258:
		if (!s_pPlayerNamesList) s_pPlayerNamesList = (image_base + 0x2F1F678);
		m_sPedFactory = (image_base + 0x25B14B0);
		s_pViewAngles = (image_base + 0x201ED50);
		s_pViewPort = (image_base + 0x201DBA0);
		s_pSwapChain = (image_base + 0x2D2CAA0);
		

		m_pPedTask = 0x144B;
		m_pEntityType = 0x1098;
		m_pArmor = 0x150C;
		m_pWeaponManager = 0x10B8;
		m_pPlayerInfo = 0x10A8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x13C0;
		m_pEnghealth = 0x08E8;
		m_pNetid = 0xE8;
		m_pPlayerName = 0xA4;
		m_pVehMgr = 0x0D10;
		m_pVelocity = 0x300;
		m_pGravity = 0xC8C;
		m_pHandlingData = 0x960;
		m_pFrameFlags = 0x270;
		m_pConfigFlags = 0x1444;
		m_pBoneOffset = 0x410;
		break;
	case 3095:
		if (!s_pPlayerNamesList) s_pPlayerNamesList = (image_base + 0x2F1F678);
		m_sPedFactory = (image_base + 0x2593320);
		s_pViewAngles = (image_base + 0x20025B8);
		s_pViewPort = (image_base + 0x20019E0);
		s_pSwapChain = (image_base + 0x2D00BA0);

		m_pPedTask = 0x144B;
		m_pEntityType = 0x1098;
		m_pArmor = 0x150C;
		m_pWeaponManager = 0x10B8;
		m_pPlayerInfo = 0x10A8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x13C0;
		m_pEnghealth = 0x08E8;
		m_pNetid = 0xE8;
		m_pPlayerName = 0xA4;
		m_pVehMgr = 0x0D10;
		m_pVelocity = 0x300;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x960;
		m_pFrameFlags = 0x270;
		m_pConfigFlags = 0x1444;
		m_pBoneOffset = 0x410;

		break;
	case 2944:
		m_sPedFactory = (image_base + 0x257BEA0);
		s_pViewAngles = (image_base + 0x1FEB698);
		s_pViewPort = (image_base + 0x1FEAAC0);
		s_pSwapChain = (image_base + 0x2CAA600);

		m_pPedTask = 0x144B;
		m_pEntityType = 0x1098;
		m_pArmor = 0x150C;
		m_pWeaponManager = 0x10B8;
		m_pPlayerInfo = 0x10A8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x1370;
		m_pEnghealth = 0x08E8;
		m_pNetid = 0xE8;
		m_pPlayerName = 0xA4;
		m_pVehMgr = 0x0D10;
		m_pVelocity = 0x300;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x0918;
		m_pFrameFlags = 0x270;
		m_pConfigFlags = 0x1444;
		m_pBoneOffset = 0x410;

		break;
	case 2802:
		m_sPedFactory = (image_base + 0x254D448);
		s_pViewAngles = (image_base + 0x1FBCCD8);
		s_pViewPort = (image_base + 0x1FBC100);
		s_pSwapChain = (image_base + 0x2C6B100);

		m_pPedTask = 0x144B;
		m_pEntityType = 0x1098;
		m_pArmor = 0x150C;
		m_pWeaponManager = 0x10B8;
		m_pPlayerInfo = 0x10A8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x1370;
		m_pEnghealth = 0x08E8;
		m_pNetid = 0x88;
		m_pPlayerName = 0x84;
		m_pVehMgr = 0x0D10;
		m_pVelocity = 0x300;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x0918;
		m_pFrameFlags = 0x270;
		m_pConfigFlags = 0x1444;
		m_pBoneOffset = 0x410;

		break;
	case 2699:
		m_sPedFactory = (image_base + 0x26684D8);
		s_pViewAngles = (image_base + 0x20D9868);
		s_pViewPort = (image_base + 0x20D8C90);
		s_pSwapChain = (image_base + 0x2D38D88);

		m_pPedTask = 0x146B;
		m_pEntityType = 0x10B8;
		m_pArmor = 0x1530;
		m_pWeaponManager = 0x10D8;
		m_pPlayerInfo = 0x10C8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x1390;
		m_pEnghealth = 0x908;
		m_pNetid = 0x88;
		m_pPlayerName = 0x84;
		m_pVehMgr = 0x0D30;
		m_pVelocity = 0x320;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x938;
		m_pFrameFlags = 0x0218;
		m_pConfigFlags = 0x1464;
		m_pBoneOffset = 0x430;

		break;
	case 2612:
		m_sPedFactory = (image_base + 0x2567DB0);
		s_pViewAngles = (image_base + 0x1FD9148);
		s_pViewPort = (image_base + 0x1FD8570);
		s_pSwapChain = (image_base + 0x2BCB7D0);

		m_pPedTask = 0x146B;
		m_pEntityType = 0x10B8;
		m_pArmor = 0x1530;
		m_pWeaponManager = 0x10D8;
		m_pPlayerInfo = 0x10C8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x1390;
		m_pEnghealth = 0x908;
		m_pNetid = 0x88;
		m_pPlayerName = 0x84;
		m_pVehMgr = 0x0D30;
		m_pVelocity = 0x320;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x938;
		m_pFrameFlags = 0x0218;
		m_pConfigFlags = 0x1464;
		m_pBoneOffset = 0x430;

		break;
	case 2545:
		m_sPedFactory = (image_base + 0x25667E8);
		s_pViewAngles = (image_base + 0x1FD7B48);
		s_pViewPort = (image_base + 0x1FD6F70);
		s_pSwapChain = (image_base + 0x2BCA050);

		m_pPedTask = 0x146B;
		m_pEntityType = 0x10B8;
		m_pArmor = 0x14E0 + 0x50;
		m_pWeaponManager = 0x10D8;
		m_pPlayerInfo = 0x10C8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x1390;
		m_pEnghealth = 0x908;
		m_pNetid = 0x88;
		m_pPlayerName = 0x84;
		m_pVehMgr = 0x0D30;
		m_pVelocity = 0x320;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x938;
		m_pFrameFlags = 0x0218;
		m_pConfigFlags = 0x1464;
		m_pBoneOffset = 0x430;

		break;
	case 2372:
		m_sPedFactory = (image_base + 0x252DCD8);
		s_pViewAngles = (image_base + 0x1F9F5C8);
		s_pViewPort = (image_base + 0x1F9E9F0);
		s_pSwapChain = (image_base + 0x2B8F708);

		m_pPedTask = 0x146B;
		m_pEntityType = 0x10B8;
		m_pArmor = 0x14E0;
		m_pWeaponManager = 0x10D8;
		m_pPlayerInfo = 0x10C8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x1390;
		m_pEnghealth = 0x908;
		m_pNetid = 0x88;
		m_pPlayerName = 0x84;
		m_pVehMgr = 0x0D30;
		m_pVelocity = 0x320;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x938;
		m_pFrameFlags = 0x0218;
		m_pConfigFlags = 0x1414;
		m_pBoneOffset = 0x430;

		break;
	case 2189:
		m_sPedFactory = (image_base + 0x24E6D90);
		s_pViewAngles = (image_base + 0x1F89498);
		s_pViewPort = (image_base + 0x1F888C0);
		s_pSwapChain = (image_base + 0x2B580C0);

		m_pPedTask = 0x146B;
		m_pEntityType = 0x10B8;
		m_pArmor = 0x14E0;
		m_pWeaponManager = 0x10D8;
		m_pPlayerInfo = 0x10C8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x1390;
		m_pEnghealth = 0x908;
		m_pNetid = 0x68;
		m_pPlayerName = 0x84;
		m_pVehMgr = 0x0D30;
		m_pVelocity = 0x320;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x938;
		m_pFrameFlags = 0x0218;
		m_pConfigFlags = 0x1414;
		m_pBoneOffset = 0x430;

		break;
	case 2060:
		m_sPedFactory = (image_base + 0x24C8858);
		s_pViewAngles = (image_base + 0x1F6B3C0);
		s_pViewPort = (image_base + 0x1F6A7E0);
		s_pSwapChain = (image_base + 0x2B2C9C0);

		m_pPedTask = 0x146B;
		m_pEntityType = 0x10A8;
		m_pArmor = 0x14E0;
		m_pWeaponManager = 0x10D8;
		m_pPlayerInfo = 0x10B8;
		m_pRecoil = 0x2F4;
		m_pSpread = 0x84;
		m_pReloadMult = 0x134;
		m_pRange = 0x28C;
		m_pDoorstatus = 0x1390;
		m_pEnghealth = 0x908;
		m_pNetid = 0x68;
		m_pPlayerName = 0x84;
		m_pVehMgr = 0x0D28;
		m_pVelocity = 0x320;
		m_pGravity = 0xC5C;
		m_pHandlingData = 0x908;
		m_pFrameFlags = 0x0218;
		m_pConfigFlags = 0x1414;
		m_pBoneOffset = 0x430;

		break;
	case 1604:
		m_sPedFactory = (image_base + 0x247F840);
		s_pViewAngles = (image_base + 0x2088360);
		s_pViewPort = (image_base + 0x2087780);
		s_pSwapChain = (image_base + 0x2AD7448);

		m_pPedTask = 0x146B;
		m_pEntityType = 0x10A8;
		m_pArmor = 0x14B8;
		m_pWeaponManager = 0x10C8;
		m_pPlayerInfo = 0x10B8;
		m_pRecoil = 0x2E8;
		m_pSpread = 0x74;
		m_pReloadMult = 0x12C;
		m_pRange = 0x25C;
		m_pDoorstatus = 0x1350;
		m_pEnghealth = 0x908;
		m_pNetid = 0x60;
		m_pPlayerName = 0x7C;
		m_pVehMgr = 0x0D28;
		m_pHandlingData = 0x0918;
		m_pVelocity = 0x300;
		m_pGravity = 0x0C8C;
		m_pFrameFlags = 0x01F8;
		m_pConfigFlags = 0x1414;
		m_pBoneOffset = 0x430;

		break;
	}
	
	if (dwThreadCollectionPtr && activeThreadTlsOffset) {
		g_tls_hook->Initialize(dwThreadCollectionPtr);
	}
	
	s_pSwapChain = Scanner::Get()->Scan(NULL, sk("48 8B 0D ? ? ? ? 48 8B 01 44 8D 43 01 33 D2 FF 50 40 8B C8"), NULL, 7);
}

void Address::GetPlayerNameInternal(int netid, char* outName, size_t outSize) {
    __try {
        uintptr_t baseAddr = s_pPlayerNamesList;

        if (!baseAddr) {
            uintptr_t playernames = (uintptr_t)GetModuleHandleA(sk("citizen-playernames-five.dll"));
            if (playernames) baseAddr = playernames + 0x30D98;
        }
        if (!baseAddr) return;

        if (IsBadReadPtr((void*)baseAddr, sizeof(uintptr_t) + sizeof(int))) return;

        uintptr_t PlayerNamesArray = *(uintptr_t*)baseAddr;
        if (!PlayerNamesArray) return;

        int LastPlayer = *(int*)(baseAddr + 0x8);
        if (LastPlayer <= 0 || LastPlayer > 500) return;

        uintptr_t* pList = (uintptr_t*)(PlayerNamesArray + 0x8);
        if (IsBadReadPtr(pList, sizeof(uintptr_t))) return;
        
        auto list = *pList;
        for (int i = 0; i < LastPlayer; i++) {
            if (!list || IsBadReadPtr((void*)list, 0x40)) break;
            
            int id = *(int*)(list + 0x10);
            if (netid == id) {
                int capacity = *(int*)(list + 0x30);
                char* namePtr = nullptr;
                if (capacity < 16) {
                    namePtr = (char*)(list + 0x18);
                } else {
                    namePtr = *(char**)(list + 0x18);
                }
                if (namePtr && !IsBadStringPtrA(namePtr, outSize)) {
                    strncpy_s(outName, outSize, namePtr, _TRUNCATE);
                }
                break;
            }
            list = *(uintptr_t*)(list + 8);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

std::string Address::GetPlayerNameFromInfo(uint64_t playerinfo) {
    if (!playerinfo) return "";
    __try {
        if (IsBadReadPtr((void*)playerinfo, m_pPlayerName + 20)) return "";
        const char* namePtr = (const char*)(playerinfo + m_pPlayerName);
        if (!namePtr || IsBadStringPtrA(namePtr, 20)) return "";
        char buf[24] = {};
        strncpy_s(buf, sizeof(buf), namePtr, _TRUNCATE);
        buf[20] = '\0';
        if (buf[0] == '\0') return "";
        return std::string(buf);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return "";
}

std::string Address::GetPlayerNameByNetId(int netid) {
    if (netid == 0) return "NPC";
    char nameBuffer[64] = "NPC";
    GetPlayerNameInternal(netid, nameBuffer, sizeof(nameBuffer));
    return std::string(nameBuffer);
}