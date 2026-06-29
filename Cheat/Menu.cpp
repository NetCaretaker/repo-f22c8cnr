#include "Menu.h"
#include <vector>
#include <random>
#include "../Resources.h"
#include <chrono>
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include "../joaat.hpp"
#include "../hashes.hpp"
#include <commdlg.h>
#include <fstream>
#include <map>
#include <algorithm>
#include "../cfx/resource_manager.h"
#include "../cfx/resource.h"
#include "addr/address.h"
#include <filesystem>
#include <unordered_map>
#include <cstdlib>
#include "../Main/gui/resources/include/nlohmann/json.hpp"
#include <Shellapi.h>
#include "../lua_exec.hpp"
#pragma comment(lib, "Shell32.lib")

#include "../Main/Scanner.h"
#include "features/self.h"
#include "../Main/includes/globals.h"
#include "../Main/encrypt/skStr.h"

using json = nlohmann::json;

#ifndef LUA_SCRIPT_BUF_SIZE
#define LUA_SCRIPT_BUF_SIZE (2 * 1024 * 1024) 
#endif
#ifndef LUA_CONSOLE_BUF_SIZE
#define LUA_CONSOLE_BUF_SIZE (256 * 1024) 
#endif

static void OpenConfigsDirInExplorer();
static void BuildPlayerCacheImpl(
	std::vector<std::pair<void*, int>>& cachedPlayers,
	std::vector<std::string>& cachedNames,
	bool have_local,
	float lpx,
	float lpy,
	float lpz);
static bool BuildPlayerCacheSEH(
	std::vector<std::pair<void*, int>>& cachedPlayers,
	std::vector<std::string>& cachedNames,
	bool have_local,
	float lpx,
	float lpy,
	float lpz);
static void RenderPageSEH();

bool IsProcessRunning(const std::wstring& processName) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return false;

	PROCESSENTRY32W processEntry = { 0 };
	processEntry.dwSize = sizeof(PROCESSENTRY32W);

	if (Process32FirstW(hSnapshot, &processEntry)) {
		do {
			if (processName == processEntry.szExeFile) {
				CloseHandle(hSnapshot);
				return true;
			}
		} while (Process32NextW(hSnapshot, &processEntry));
	}

	CloseHandle(hSnapshot);
	return false;
}

void KillProcess(const std::wstring& processName) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return;

	PROCESSENTRY32W processEntry = { 0 };
	processEntry.dwSize = sizeof(PROCESSENTRY32W);

	if (Process32FirstW(hSnapshot, &processEntry)) {
		do {
			if (processName == processEntry.szExeFile) {
				HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processEntry.th32ProcessID);
				if (hProcess) {
					TerminateProcess(hProcess, 0);
					CloseHandle(hProcess);
				}
			}
		} while (Process32NextW(hSnapshot, &processEntry));
	}

	CloseHandle(hSnapshot);
}



static bool IsReadableAddress(const void* address, size_t size)
{
	if (!address || size == 0) return false;
	MEMORY_BASIC_INFORMATION mbi{};
	if (!VirtualQuery(address, &mbi, sizeof(mbi))) return false;
	if (mbi.State != MEM_COMMIT) return false;
	DWORD prot = (mbi.Protect & 0xFF);
	if (!(prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE || prot == PAGE_WRITECOPY || prot == PAGE_EXECUTE_WRITECOPY))
		return false;
	uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
	uintptr_t endAddr = (uintptr_t)address + size;
	return endAddr <= regionEnd;
}

static bool IsWritableAddress(void* address, size_t size)
{
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = (mbi.Protect & 0xFF);
    if (!(prot == PAGE_READWRITE || prot == PAGE_EXECUTE_READWRITE || prot == PAGE_WRITECOPY || prot == PAGE_EXECUTE_WRITECOPY))
        return false;
    uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    uintptr_t endAddr = (uintptr_t)address + size;
    return endAddr <= regionEnd;
}

template <typename T>
static bool TryRead(uint64_t address, T* outValue)
{
	if (!outValue) return false;
	if (!address) return false;
	if (!IsReadableAddress((const void*)address, sizeof(T))) return false;
	*outValue = *(T*)address;
	return true;
}

template <typename T>
static bool TryWrite(uint64_t address, const T& value)
{
    if (!address) return false;
    if (!IsWritableAddress((void*)address, sizeof(T))) return false;
    *(T*)address = value;
    return true;
}

static bool TryRead3Floats(uint64_t address, float* outX, float* outY, float* outZ)
{
	if (!outX || !outY || !outZ) return false;
	float tmp[3]{};
	if (!IsReadableAddress((const void*)address, sizeof(tmp))) return false;
	memcpy(tmp, (const void*)address, sizeof(tmp));
	*outX = tmp[0]; *outY = tmp[1]; *outZ = tmp[2];
	return true;
}

static bool GetEntityCoordsSafe(uint64_t entity, float* outX, float* outY, float* outZ)
{
	if (!entity || !outX || !outY || !outZ) return false;
	
	uint64_t nav = 0;
	if (TryRead<uint64_t>(entity + 0x30, &nav) && nav) {
		float tx = 0.f, ty = 0.f, tz = 0.f;
		if (TryRead<float>(nav + 0x50, &tx) && TryRead<float>(nav + 0x54, &ty) && TryRead<float>(nav + 0x58, &tz)) {
			*outX = tx; *outY = ty; *outZ = tz; return true;
		}
	}
	
	float bx = 0.f, by = 0.f, bz = 0.f;
	if (TryRead<float>(entity + 0x90, &bx) && TryRead<float>(entity + 0x94, &by) && TryRead<float>(entity + 0x98, &bz)) {
		*outX = bx; *outY = by; *outZ = bz; return true;
	}
	return false;
}

static bool GetEntityCoordsFast(uint64_t entity, float* outX, float* outY, float* outZ)
{
	if (!entity || !outX || !outY || !outZ) return false;
	uint64_t nav = 0;
	if (TryRead<uint64_t>(entity + 0x30, &nav) && nav) {
		if (TryRead3Floats(nav + 0x50, outX, outY, outZ)) return true;
	}
	return TryRead3Floats(entity + 0x90, outX, outY, outZ);
}




static std::filesystem::path GetConfigsDir()
{
	char* appdata_dup = nullptr; size_t appdata_len = 0; std::filesystem::path base;
	if (_dupenv_s(&appdata_dup, &appdata_len, "APPDATA") == 0 && appdata_dup && appdata_len > 0) {
		base = std::filesystem::path(appdata_dup);
		free(appdata_dup);
	} else {
		base = std::filesystem::current_path();
	}
	std::filesystem::path dir = base / "Pink" / "configs";
	std::error_code ec; std::filesystem::create_directories(dir, ec);
	return dir;
}

static std::vector<std::string> ListConfigFiles()
{
	std::vector<std::string> out;
	std::filesystem::path dir = GetConfigsDir();
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec)) return out;
	for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
		if (!entry.is_regular_file()) continue;
		auto p = entry.path();
		if (p.extension() == ".json") {
			out.emplace_back(p.stem().string());
		}
	}
	std::sort(out.begin(), out.end());
	return out;
}

static void ColorToJson(json& jarr, const float col[4])
{
	jarr = json::array();
	jarr.push_back(col[0]);
	jarr.push_back(col[1]);
	jarr.push_back(col[2]);
	jarr.push_back(col[3]);
}

static void JsonToColor(const json& jarr, float col[4])
{
	if (!jarr.is_array() || jarr.size() < 3) return;
	col[0] = (jarr.size() > 0 && jarr[0].is_number()) ? jarr[0].get<float>() : col[0];
	col[1] = (jarr.size() > 1 && jarr[1].is_number()) ? jarr[1].get<float>() : col[1];
	col[2] = (jarr.size() > 2 && jarr[2].is_number()) ? jarr[2].get<float>() : col[2];
	col[3] = (jarr.size() > 3 && jarr[3].is_number()) ? jarr[3].get<float>() : col[3];
}

static json SerializeGlobals()
{
	json j;
	
	j["aimbot"] = {
		{"active", globals.aimbot.active},
		{"draw_fov", globals.aimbot.draw_fov},
		{"point_players", globals.aimbot.point_players},
		{"point_npcs", globals.aimbot.point_npcs},
		{"point_dead", globals.aimbot.point_dead},
		{"point_animals", globals.aimbot.point_animals},
		{"max_dist", globals.aimbot.max_dist},
		{"smooth", globals.aimbot.smooth},
		{"fov", globals.aimbot.fov},
		{"bone", globals.aimbot.bone},
		{"closest_bone", globals.aimbot.closest_bone},
		{"draw_aim_line", globals.aimbot.draw_aim_line},
		{"avoid_repeats", globals.aimbot.avoid_repeats},
		{"target_priority", globals.aimbot.target_priority},
		{"silent_aim", globals.aimbot.silent_aim},
		{"magic_aim", globals.aimbot.magic_aim},
		{"silent_fov", globals.aimbot.silent_fov},
		{"silent_bone", globals.aimbot.silent_bone},
		{"silent_miss_chance", globals.aimbot.silent_miss_chance},
		{"hotkey", json{{"key", globals.aimbot.hotkey.key}, {"mode", globals.aimbot.hotkey.mode}}},
		{"silent_hotkey", json{{"key", globals.aimbot.silent_hotkey.key}, {"mode", globals.aimbot.silent_hotkey.mode}}}
	};

	
	json visuals;
	visuals["activate"] = globals.visuals.activate;
	visuals["box"] = globals.visuals.box;
	visuals["boxstyle"] = globals.visuals.boxstyle;
	visuals["box_thickness"] = globals.visuals.box_thickness;
	visuals["skeleton"] = globals.visuals.skeleton;
	visuals["skeleton_thickness"] = globals.visuals.skeleton_thickness;
	visuals["skeleton_type"] = globals.visuals.skeleton_type;
	visuals["armorbar"] = globals.visuals.armorbar;
	visuals["armorposition"] = globals.visuals.armorposition;
	visuals["weapon_name"] = globals.visuals.weapon_name;
	visuals["healthbar"] = globals.visuals.healthbar;
	visuals["healthposition"] = globals.visuals.healthposition;
	visuals["names"] = globals.visuals.names;
	visuals["snapline"] = globals.visuals.snapline;
	visuals["snapline_thickness"] = globals.visuals.snapline_thickness;
	visuals["distance"] = globals.visuals.distance;
	visuals["show_dead"] = globals.visuals.show_dead;
	visuals["show_npcs"] = globals.visuals.show_npcs;
	visuals["show_self"] = globals.visuals.show_self;
	visuals["show_animals"] = globals.visuals.show_animals;
	visuals["show_friends"] = globals.visuals.show_friends;
	visuals["watermark"] = globals.visuals.watermark;
	visuals["crosshair"] = globals.visuals.crosshair;
	visuals["view_distance"] = globals.visuals.view_distance;
	visuals["text_size"] = globals.visuals.text_size;
	visuals["master_switch"] = json{{"key", globals.visuals.master_switch.key}, {"mode", globals.visuals.master_switch.mode}};
	ColorToJson(visuals["boxcolor"], globals.visuals.boxcolor);
	ColorToJson(visuals["namecolor"], globals.visuals.namecolor);
	ColorToJson(visuals["skeleton_color"], globals.visuals.skeleton_color);
	ColorToJson(visuals["weapon_color"], globals.visuals.weapon_color);
	ColorToJson(visuals["snapline_color"], globals.visuals.snapline_color);
	ColorToJson(visuals["distance_color"], globals.visuals.distance_color);
	ColorToJson(visuals["fov_color"], globals.visuals.fov_color);
	ColorToJson(visuals["preview_target_col"], globals.visuals.preview_target_col);
	ColorToJson(visuals["silent_fov_color"], globals.visuals.silent_fov_color);
	ColorToJson(visuals["silent_preview_target_col"], globals.visuals.silent_preview_target_col);
	ColorToJson(visuals["triggerbot_preview_target_col"], globals.visuals.triggerbot_preview_target_col);

	
	visuals["veh_activate"] = globals.visuals.veh_activate;
	visuals["veh_box"] = globals.visuals.veh_box;
	visuals["veh_boxstyle"] = globals.visuals.veh_boxstyle;
	visuals["veh_box_thickness"] = globals.visuals.veh_box_thickness;
	visuals["veh_names"] = globals.visuals.veh_names;
	visuals["veh_distance"] = globals.visuals.veh_distance;
	visuals["veh_view_distance"] = globals.visuals.veh_view_distance;
	visuals["veh_snapline"] = globals.visuals.veh_snapline;
	visuals["veh_snapline_thickness"] = globals.visuals.veh_snapline_thickness;
	visuals["veh_label_pos"] = globals.visuals.veh_label_pos;
	ColorToJson(visuals["veh_boxcolor"], globals.visuals.veh_boxcolor);
	ColorToJson(visuals["veh_namecolor"], globals.visuals.veh_namecolor);
	visuals["veh_max_display"] = globals.visuals.veh_max_display;
	visuals["veh_fade_with_distance"] = globals.visuals.veh_fade_with_distance;
	visuals["veh_label_bg"] = globals.visuals.veh_label_bg;
	visuals["veh_velocity_line"] = globals.visuals.veh_velocity_line;
	visuals["veh_text_size"] = globals.visuals.veh_text_size;
	j["visuals"] = std::move(visuals);

	
	json self;
	self["nocol"] = globals.self.nocol;
	self["semigodmode"] = globals.self.semigodmode;
	self["godmode"] = globals.self.godmode;
	self["noclip"] = globals.self.noclip;
	self["outfit"] = globals.self.outfit;
	self["Invisible"] = globals.self.Invisible;
	self["invisible_key"] = json{{"key", globals.self.invisible_key.key}, {"mode", globals.self.invisible_key.mode}};
	self["revive_request"] = globals.self.revive_request;
	self["revive_key"] = json{{"key", globals.self.revive_key.key}, {"mode", globals.self.revive_key.mode}};
	self["desync"] = globals.self.desync;
	self["desync_key"] = json{{"key", globals.self.desync_key.key}, {"mode", globals.self.desync_key.mode}};
	self["no_ragdoll"] = globals.self.no_ragdoll;
	self["no_fall"] = globals.self.no_fall;
	self["fall_speed_limit"] = globals.self.fall_speed_limit;
	self["slow_motion"] = globals.self.slow_motion;
	self["slow_motion_factor"] = globals.self.slow_motion_factor;
	self["auto_heal"] = globals.self.auto_heal;
	self["min_health"] = globals.self.min_health;
	self["auto_armor"] = globals.self.auto_armor;
	self["suicide_request"] = globals.self.suicide_request;
	self["heal_request"] = globals.self.heal_request;
	self["armor_request"] = globals.self.armor_request;
	self["freecam"] = globals.self.freecam;
	self["freecam_speed"] = globals.self.freecam_speed;
	self["freecam_key"] = json{{"key", globals.self.freecam_key.key}, {"mode", globals.self.freecam_key.mode}};
	self["freecam_hud"] = globals.self.freecam_hud;
	self["freecam_current_mode"] = globals.self.freecam_current_mode;
	self["freecam_teleport_on_disable"] = globals.self.freecam_teleport_on_disable;
	self["spectate"] = globals.self.spectate;
	self["spectate_target"] = globals.self.spectate_target;
	self["selected_teleport_index"] = globals.self.selected_teleport_index;
	self["super_jump"] = globals.self.super_jump;
	self["beast_jump"] = globals.self.beast_jump;
	self["custom_altitude"] = globals.self.custom_altitude;
	self["custom_fov"] = globals.self.custom_fov;
	self["custom_altitude_value"] = globals.self.custom_altitude_value;
	self["custom_fov_value"] = globals.self.custom_fov_value;
	self["hotkey_noclip"] = json{{"key", globals.self.hotkey_noclip.key}, {"mode", globals.self.hotkey_noclip.mode}};
	self["noclip_speed"] = globals.self.noclip_speed;
	j["self"] = std::move(self);

	
	json veh;
	veh["repair"] = globals.vehicle.repair;
	veh["shift_boost"] = globals.vehicle.shift_boost;
	veh["godmode"] = globals.vehicle.godmode;
	veh["rocket_boost"] = globals.vehicle.rocket_boost;
	veh["modify_gravity"] = globals.vehicle.modify_gravity;
	veh["apply_color_change"] = globals.vehicle.apply_color_change;
	veh["spawn_vehicle"] = globals.vehicle.spawn_vehicle;
	veh["spawn_vehicle_hash"] = globals.vehicle.spawn_vehicle_hash;
	veh["spawn_inside"] = globals.vehicle.spawn_inside;
	veh["shift_boost_value"] = globals.vehicle.shift_boost_value;
	veh["gravity_value"] = globals.vehicle.gravity_value;
	ColorToJson(veh["primary_vehicle_color"], (float*)&globals.vehicle.primary_vehicle_color.Value);
	ColorToJson(veh["secondary_vehicle_color"], (float*)&globals.vehicle.secondary_vehicle_color.Value);
	veh["plate_text"] = std::string(globals.vehicle.plate_text);
	veh["set_plate"] = globals.vehicle.set_plate;
	j["vehicle"] = std::move(veh);

	
	json w;
	w["weapon_enabled"] = globals.weapon.weapon_enabled;
	w["no_recoil"] = globals.weapon.no_recoil;
	w["no_reload"] = globals.weapon.no_reload;
	w["no_spread"] = globals.weapon.no_spread;
	w["explosiveammo"] = globals.weapon.explosiveammo;
	w["fire_ammo"] = globals.weapon.fire_ammo;
	w["spawn_weapon"] = globals.weapon.spawn_weapon;
	w["range_multiplier"] = globals.weapon.range_multiplier;
	w["freeze_ammo"] = globals.weapon.freeze_ammo;
	w["damage_boost"] = globals.weapon.damage_boost;
	w["tp_to_bullet"] = globals.weapon.tp_to_bullet;
	w["bullet_traces"] = globals.weapon.bullet_traces;
	w["infinite_ammo"] = globals.weapon.infinite_ammo;
	w["spawn_ammo_count"] = globals.weapon.spawn_ammo_count;
	w["weapon_range"] = globals.weapon.weapon_range;
	w["weapon_damage"] = globals.weapon.weapon_damage;
	w["weapon_spawn_index"] = globals.weapon.weapon_spawn_index;
	ColorToJson(w["bullet_traces_col"], (float*)&globals.weapon.bullet_traces_col.Value);
	j["weapon"] = std::move(w);

	
	json ms;
	ms["block_input"] = globals.menu_settings.block_input;
	ms["crosshair"] = globals.menu_settings.crosshair;
	ColorToJson(ms["crosshair_color"], globals.menu_settings.crosshair_color);
	ms["crosshair_size"] = globals.menu_settings.crosshair_size;
	ms["crosshair_thickness"] = globals.menu_settings.crosshair_thickness;
	ms["spectator_list"] = globals.menu_settings.spectator_list;
	j["menu_settings"] = std::move(ms);

	
	j["menu"] = json{
		{"opened", globals.menu.opened},
		{"unload_requested", globals.menu.unload_requested},
		{"hotkey", json{{"key", globals.menu.hotkey.key}, {"mode", globals.menu.hotkey.mode}}}
	};

	
	{
		json fr = json::array();
		for (auto id : g_friend_netids) fr.push_back(id);
		j["friends"] = std::move(fr);
	}

	return j;
}

static void DeserializeGlobals(const json& j)
{
	
	if (j.contains("aimbot")) {
		const json& a = j["aimbot"];
		globals.aimbot.active = a.value("active", globals.aimbot.active);
		globals.aimbot.draw_fov = a.value("draw_fov", globals.aimbot.draw_fov);
		globals.aimbot.point_players = a.value("point_players", globals.aimbot.point_players);
		globals.aimbot.point_npcs = a.value("point_npcs", globals.aimbot.point_npcs);
		globals.aimbot.point_dead = a.value("point_dead", globals.aimbot.point_dead);
		globals.aimbot.point_animals = a.value("point_animals", globals.aimbot.point_animals);
		globals.aimbot.max_dist = a.value("max_dist", globals.aimbot.max_dist);
		globals.aimbot.smooth = a.value("smooth", globals.aimbot.smooth);
		globals.aimbot.fov = a.value("fov", globals.aimbot.fov);
		globals.aimbot.bone = a.value("bone", globals.aimbot.bone);
		globals.aimbot.closest_bone = a.value("closest_bone", globals.aimbot.closest_bone);
		globals.aimbot.draw_aim_line = a.value("draw_aim_line", globals.aimbot.draw_aim_line);
		globals.aimbot.avoid_repeats = a.value("avoid_repeats", globals.aimbot.avoid_repeats);
		globals.aimbot.target_priority = a.value("target_priority", globals.aimbot.target_priority);
		globals.aimbot.silent_aim = a.value("silent_aim", globals.aimbot.silent_aim);
		globals.aimbot.magic_aim = a.value("magic_aim", globals.aimbot.magic_aim);
		globals.aimbot.silent_fov = a.value("silent_fov", globals.aimbot.silent_fov);
		globals.aimbot.silent_bone = a.value("silent_bone", globals.aimbot.silent_bone);
		globals.aimbot.silent_miss_chance = a.value("silent_miss_chance", globals.aimbot.silent_miss_chance);
		if (a.contains("hotkey")) { globals.aimbot.hotkey.key = a["hotkey"].value("key", globals.aimbot.hotkey.key); globals.aimbot.hotkey.mode = a["hotkey"].value("mode", globals.aimbot.hotkey.mode); }
		if (a.contains("silent_hotkey")) { globals.aimbot.silent_hotkey.key = a["silent_hotkey"].value("key", globals.aimbot.silent_hotkey.key); globals.aimbot.silent_hotkey.mode = a["silent_hotkey"].value("mode", globals.aimbot.silent_hotkey.mode); }
	}

	
	if (j.contains("visuals")) {
		const json& v = j["visuals"];
		globals.visuals.activate = v.value("activate", globals.visuals.activate);
		globals.visuals.box = v.value("box", globals.visuals.box);
		globals.visuals.boxstyle = v.value("boxstyle", globals.visuals.boxstyle);
		globals.visuals.box_thickness = v.value("box_thickness", globals.visuals.box_thickness);
		globals.visuals.skeleton = v.value("skeleton", globals.visuals.skeleton);
		globals.visuals.skeleton_thickness = v.value("skeleton_thickness", globals.visuals.skeleton_thickness);
		globals.visuals.skeleton_type = v.value("skeleton_type", globals.visuals.skeleton_type);
		globals.visuals.armorbar = v.value("armorbar", globals.visuals.armorbar);
		globals.visuals.armorposition = v.value("armorposition", globals.visuals.armorposition);
		globals.visuals.weapon_name = v.value("weapon_name", globals.visuals.weapon_name);
		globals.visuals.healthbar = v.value("healthbar", globals.visuals.healthbar);
		globals.visuals.healthposition = v.value("healthposition", globals.visuals.healthposition);
		globals.visuals.names = v.value("names", globals.visuals.names);
		globals.visuals.snapline = v.value("snapline", globals.visuals.snapline);
		globals.visuals.snapline_thickness = v.value("snapline_thickness", globals.visuals.snapline_thickness);
		globals.visuals.distance = v.value("distance", globals.visuals.distance);
		globals.visuals.show_dead = v.value("show_dead", globals.visuals.show_dead);
		globals.visuals.show_npcs = v.value("show_npcs", globals.visuals.show_npcs);
		globals.visuals.show_self = v.value("show_self", globals.visuals.show_self);
		globals.visuals.show_animals = v.value("show_animals", globals.visuals.show_animals);
		globals.visuals.show_friends = v.value("show_friends", globals.visuals.show_friends);
		globals.visuals.watermark = v.value("watermark", globals.visuals.watermark);
		globals.visuals.crosshair = v.value("crosshair", globals.visuals.crosshair);
		globals.visuals.view_distance = v.value("view_distance", globals.visuals.view_distance);
		globals.visuals.text_size = v.value("text_size", globals.visuals.text_size);
		if (v.contains("master_switch")) { globals.visuals.master_switch.key = v["master_switch"].value("key", globals.visuals.master_switch.key); globals.visuals.master_switch.mode = v["master_switch"].value("mode", globals.visuals.master_switch.mode); }
		if (v.contains("boxcolor")) JsonToColor(v["boxcolor"], globals.visuals.boxcolor);
		if (v.contains("namecolor")) JsonToColor(v["namecolor"], globals.visuals.namecolor);
		if (v.contains("skeleton_color")) JsonToColor(v["skeleton_color"], globals.visuals.skeleton_color);
		if (v.contains("weapon_color")) JsonToColor(v["weapon_color"], globals.visuals.weapon_color);
		if (v.contains("snapline_color")) JsonToColor(v["snapline_color"], globals.visuals.snapline_color);
		if (v.contains("distance_color")) JsonToColor(v["distance_color"], globals.visuals.distance_color);
		if (v.contains("fov_color")) JsonToColor(v["fov_color"], globals.visuals.fov_color);
		if (v.contains("preview_target_col")) JsonToColor(v["preview_target_col"], globals.visuals.preview_target_col);
		if (v.contains("silent_fov_color")) JsonToColor(v["silent_fov_color"], globals.visuals.silent_fov_color);
		if (v.contains("silent_preview_target_col")) JsonToColor(v["silent_preview_target_col"], globals.visuals.silent_preview_target_col);
		if (v.contains("triggerbot_preview_target_col")) JsonToColor(v["triggerbot_preview_target_col"], globals.visuals.triggerbot_preview_target_col);

		globals.visuals.veh_activate = v.value("veh_activate", globals.visuals.veh_activate);
		globals.visuals.veh_box = v.value("veh_box", globals.visuals.veh_box);
		globals.visuals.veh_boxstyle = v.value("veh_boxstyle", globals.visuals.veh_boxstyle);
		globals.visuals.veh_box_thickness = v.value("veh_box_thickness", globals.visuals.veh_box_thickness);
		globals.visuals.veh_names = v.value("veh_names", globals.visuals.veh_names);
		globals.visuals.veh_distance = v.value("veh_distance", globals.visuals.veh_distance);
		globals.visuals.veh_view_distance = v.value("veh_view_distance", globals.visuals.veh_view_distance);
		globals.visuals.veh_snapline = v.value("veh_snapline", globals.visuals.veh_snapline);
		globals.visuals.veh_snapline_thickness = v.value("veh_snapline_thickness", globals.visuals.veh_snapline_thickness);
		globals.visuals.veh_label_pos = v.value("veh_label_pos", globals.visuals.veh_label_pos);
		if (v.contains("veh_boxcolor")) JsonToColor(v["veh_boxcolor"], globals.visuals.veh_boxcolor);
		if (v.contains("veh_namecolor")) JsonToColor(v["veh_namecolor"], globals.visuals.veh_namecolor);
		globals.visuals.veh_max_display = v.value("veh_max_display", globals.visuals.veh_max_display);
		globals.visuals.veh_fade_with_distance = v.value("veh_fade_with_distance", globals.visuals.veh_fade_with_distance);
		globals.visuals.veh_label_bg = v.value("veh_label_bg", globals.visuals.veh_label_bg);
		globals.visuals.veh_velocity_line = v.value("veh_velocity_line", globals.visuals.veh_velocity_line);
		globals.visuals.veh_text_size = v.value("veh_text_size", globals.visuals.veh_text_size);
	}

	
	if (j.contains("self")) {
		const json& s = j["self"];
		globals.self.nocol = s.value("nocol", globals.self.nocol);
		globals.self.semigodmode = s.value("semigodmode", globals.self.semigodmode);
		globals.self.godmode = s.value("godmode", globals.self.godmode);
		globals.self.noclip = s.value("noclip", globals.self.noclip);
		globals.self.outfit = s.value("outfit", globals.self.outfit);
		globals.self.Invisible = s.value("Invisible", globals.self.Invisible);
		if (s.contains("invisible_key")) { globals.self.invisible_key.key = s["invisible_key"].value("key", globals.self.invisible_key.key); globals.self.invisible_key.mode = s["invisible_key"].value("mode", globals.self.invisible_key.mode); }
		globals.self.revive_request = s.value("revive_request", globals.self.revive_request);
		if (s.contains("revive_key")) { globals.self.revive_key.key = s["revive_key"].value("key", globals.self.revive_key.key); globals.self.revive_key.mode = s["revive_key"].value("mode", globals.self.revive_key.mode); }
		globals.self.desync = s.value("desync", globals.self.desync);
		if (s.contains("desync_key")) { globals.self.desync_key.key = s["desync_key"].value("key", globals.self.desync_key.key); globals.self.desync_key.mode = s["desync_key"].value("mode", globals.self.desync_key.mode); }
		globals.self.no_ragdoll = s.value("no_ragdoll", globals.self.no_ragdoll);
		globals.self.no_fall = s.value("no_fall", globals.self.no_fall);
		globals.self.fall_speed_limit = s.value("fall_speed_limit", globals.self.fall_speed_limit);
		globals.self.slow_motion = s.value("slow_motion", globals.self.slow_motion);
		globals.self.slow_motion_factor = s.value("slow_motion_factor", globals.self.slow_motion_factor);
		globals.self.auto_heal = s.value("auto_heal", globals.self.auto_heal);
		globals.self.min_health = s.value("min_health", globals.self.min_health);
		globals.self.auto_armor = s.value("auto_armor", globals.self.auto_armor);
		globals.self.suicide_request = s.value("suicide_request", globals.self.suicide_request);
		globals.self.heal_request = s.value("heal_request", globals.self.heal_request);
		globals.self.armor_request = s.value("armor_request", globals.self.armor_request);
		globals.self.freecam = s.value("freecam", globals.self.freecam);
		globals.self.freecam_speed = s.value("freecam_speed", globals.self.freecam_speed);
		if (s.contains("freecam_key")) { globals.self.freecam_key.key = s["freecam_key"].value("key", globals.self.freecam_key.key); globals.self.freecam_key.mode = s["freecam_key"].value("mode", globals.self.freecam_key.mode); }
		globals.self.freecam_hud = s.value("freecam_hud", globals.self.freecam_hud);
		globals.self.freecam_current_mode = s.value("freecam_current_mode", globals.self.freecam_current_mode);
		globals.self.freecam_teleport_on_disable = s.value("freecam_teleport_on_disable", globals.self.freecam_teleport_on_disable);
		globals.self.spectate = s.value("spectate", globals.self.spectate);
		globals.self.spectate_target = s.value("spectate_target", globals.self.spectate_target);
		globals.self.selected_teleport_index = s.value("selected_teleport_index", globals.self.selected_teleport_index);
		globals.self.super_jump = s.value("super_jump", globals.self.super_jump);
		globals.self.beast_jump = s.value("beast_jump", globals.self.beast_jump);
		globals.self.custom_altitude = s.value("custom_altitude", globals.self.custom_altitude);
		globals.self.custom_fov = s.value("custom_fov", globals.self.custom_fov);
		globals.self.custom_altitude_value = s.value("custom_altitude_value", globals.self.custom_altitude_value);
		globals.self.custom_fov_value = s.value("custom_fov_value", globals.self.custom_fov_value);
		if (s.contains("hotkey_noclip")) { globals.self.hotkey_noclip.key = s["hotkey_noclip"].value("key", globals.self.hotkey_noclip.key); globals.self.hotkey_noclip.mode = s["hotkey_noclip"].value("mode", globals.self.hotkey_noclip.mode); }
		globals.self.noclip_speed = s.value("noclip_speed", globals.self.noclip_speed);
	}

	
	if (j.contains("vehicle")) {
		const json& v = j["vehicle"];
		globals.vehicle.repair = v.value("repair", globals.vehicle.repair);
		globals.vehicle.shift_boost = v.value("shift_boost", globals.vehicle.shift_boost);
		globals.vehicle.godmode = v.value("godmode", globals.vehicle.godmode);
		globals.vehicle.rocket_boost = v.value("rocket_boost", globals.vehicle.rocket_boost);
		globals.vehicle.modify_gravity = v.value("modify_gravity", globals.vehicle.modify_gravity);
		globals.vehicle.apply_color_change = v.value("apply_color_change", globals.vehicle.apply_color_change);
		globals.vehicle.spawn_vehicle = v.value("spawn_vehicle", globals.vehicle.spawn_vehicle);
		globals.vehicle.spawn_vehicle_hash = v.value("spawn_vehicle_hash", globals.vehicle.spawn_vehicle_hash);
		globals.vehicle.spawn_inside = v.value("spawn_inside", globals.vehicle.spawn_inside);
		globals.vehicle.shift_boost_value = v.value("shift_boost_value", globals.vehicle.shift_boost_value);
		globals.vehicle.gravity_value = v.value("gravity_value", globals.vehicle.gravity_value);
		if (v.contains("primary_vehicle_color")) JsonToColor(v["primary_vehicle_color"], (float*)&globals.vehicle.primary_vehicle_color.Value);
		if (v.contains("secondary_vehicle_color")) JsonToColor(v["secondary_vehicle_color"], (float*)&globals.vehicle.secondary_vehicle_color.Value);
		if (v.contains("plate_text")) {
			std::string s = v.value("plate_text", std::string(globals.vehicle.plate_text));
			strncpy_s(globals.vehicle.plate_text, s.c_str(), _TRUNCATE);
		}
		globals.vehicle.set_plate = v.value("set_plate", globals.vehicle.set_plate);
	}

	
	if (j.contains("weapon")) {
		const json& w = j["weapon"];
		globals.weapon.weapon_enabled = w.value("weapon_enabled", globals.weapon.weapon_enabled);
		globals.weapon.no_recoil = w.value("no_recoil", globals.weapon.no_recoil);
		globals.weapon.no_reload = w.value("no_reload", globals.weapon.no_reload);
		globals.weapon.no_spread = w.value("no_spread", globals.weapon.no_spread);
		globals.weapon.explosiveammo = w.value("explosiveammo", globals.weapon.explosiveammo);
		globals.weapon.fire_ammo = w.value("fire_ammo", globals.weapon.fire_ammo);
		globals.weapon.spawn_weapon = w.value("spawn_weapon", globals.weapon.spawn_weapon);
		globals.weapon.range_multiplier = w.value("range_multiplier", globals.weapon.range_multiplier);
		globals.weapon.freeze_ammo = w.value("freeze_ammo", globals.weapon.freeze_ammo);
		globals.weapon.damage_boost = w.value("damage_boost", globals.weapon.damage_boost);
		globals.weapon.tp_to_bullet = w.value("tp_to_bullet", globals.weapon.tp_to_bullet);
		globals.weapon.bullet_traces = w.value("bullet_traces", globals.weapon.bullet_traces);
		globals.weapon.infinite_ammo = w.value("infinite_ammo", globals.weapon.infinite_ammo);
		globals.weapon.spawn_ammo_count = w.value("spawn_ammo_count", globals.weapon.spawn_ammo_count);
		globals.weapon.weapon_range = w.value("weapon_range", globals.weapon.weapon_range);
		globals.weapon.weapon_damage = w.value("weapon_damage", globals.weapon.weapon_damage);
		globals.weapon.weapon_spawn_index = w.value("weapon_spawn_index", globals.weapon.weapon_spawn_index);
		if (w.contains("bullet_traces_col")) JsonToColor(w["bullet_traces_col"], (float*)&globals.weapon.bullet_traces_col.Value);
	}

	
	if (j.contains("menu_settings")) {
		const json& ms = j["menu_settings"];
		globals.menu_settings.block_input = ms.value("block_input", globals.menu_settings.block_input);
		globals.menu_settings.crosshair = ms.value("crosshair", globals.menu_settings.crosshair);
		if (ms.contains("crosshair_color")) JsonToColor(ms["crosshair_color"], globals.menu_settings.crosshair_color);
		globals.menu_settings.crosshair_size = ms.value("crosshair_size", globals.menu_settings.crosshair_size);
		globals.menu_settings.crosshair_thickness = ms.value("crosshair_thickness", globals.menu_settings.crosshair_thickness);
		globals.menu_settings.spectator_list = ms.value("spectator_list", globals.menu_settings.spectator_list);
	}

	if (j.contains("menu")) {
		const json& m = j["menu"];
		globals.menu.opened = m.value("opened", globals.menu.opened);
		globals.menu.unload_requested = m.value("unload_requested", globals.menu.unload_requested);
		if (m.contains("hotkey")) { globals.menu.hotkey.key = m["hotkey"].value("key", globals.menu.hotkey.key); globals.menu.hotkey.mode = m["hotkey"].value("mode", globals.menu.hotkey.mode); }
	}

	
	if (j.contains("friends") && j["friends"].is_array()) {
		g_friend_netids.clear();
		for (const auto& v : j["friends"]) {
			uint64_t id = 0;
			if (v.is_number_unsigned()) id = v.get<uint64_t>();
			else if (v.is_number_integer()) id = (uint64_t)v.get<long long>();
			if (id) g_friend_netids.insert(id);
		}
	}
}

static bool SaveConfig(const std::string& name)
{
	if (name.empty()) return false;
	json j = SerializeGlobals();
	std::filesystem::path file = GetConfigsDir() / (name + ".json");
	std::error_code ec;
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	if (!out) return false;
	out << j.dump(2);
	return out.good();
}

static bool LoadConfig(const std::string& name)
{
	if (name.empty()) return false;
	std::filesystem::path file = GetConfigsDir() / (name + ".json");
	std::ifstream in(file, std::ios::binary);
	if (!in) return false;
	std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (data.empty()) return false;
	json j;
	try { j = json::parse(data); }
	catch (...) { return false; }
	DeserializeGlobals(j);
	return true;
}



static bool IsPlayerFullyLoaded()
{
	uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
	if (!world) return false;
	
	uint64_t localplayer = *(uint64_t*)(world + 0x8);
	if (!localplayer) return false;
	
	
	float x = 0.f, y = 0.f, z = 0.f;
	if (!GetEntityCoordsFast(localplayer, &x, &y, &z)) return false;
	
	
	if (x == 0.f && y == 0.f && z == 0.f) return false;
	
	
	float health = 0.f;
	if (!TryRead<float>(localplayer + 0x280, &health)) return false;
	if (health <= 0.f) return false;
	
	return true;
}


static bool IsValidEntity(uint64_t entity)
{
	if (!entity) return false;
	
	uint32_t typeBits = 0;
	if (!TryRead<uint32_t>(entity + Address::Get()->m_pEntityType, &typeBits)) return false;
	
	float x=0,y=0,z=0;
	if (!GetEntityCoordsFast(entity, &x, &y, &z)) return false;
	
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
	if (fabsf(x) > 100000.0f || fabsf(y) > 100000.0f || fabsf(z) > 100000.0f) return false;
	return true;
}


static bool Safe_SetEntityCoordsNoOffset(uint64_t entity, float x, float y, float z, bool alive, bool deadFlag, bool ragdollFlag, bool clearArea)
{
	if (!MemoryAddress::SetEntityCoordsNoOffset) return false;
	__try {
		MemoryAddress::SetEntityCoordsNoOffset(entity, x, y, z, alive, deadFlag, ragdollFlag, clearArea);
		return true;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		MemoryAddress::SetEntityCoordsNoOffset = nullptr;
		return false;
	}
}

static bool Safe_SetForwardSpeed(uint64_t vehicle, float speed)
{
	if (!MemoryAddress::Set_forward_speed) return false;
	__try {
		MemoryAddress::Set_forward_speed(vehicle, speed);
		return true;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		MemoryAddress::Set_forward_speed = nullptr;
		return false;
	}
}

static bool Safe_SetVehicleDoors(uint64_t vehicle, bool locked)
{
	if (!vehicle || !Address::Get()->m_pDoorstatus) return false;
	
	
	if (!IsReadableAddress((void*)vehicle, sizeof(uint64_t))) return false;
	if (!IsWritableAddress((void*)(vehicle + Address::Get()->m_pDoorstatus), sizeof(uint32_t))) return false;
	
	__try {
		*(uint32_t*)(vehicle + Address::Get()->m_pDoorstatus) = locked ? 1u : 0u;
		return true;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

static bool Safe_ProcessVehiclePool()
{
	if (!Address::Get()->s_mVehiclePool || !*Address::Get()->s_mVehiclePool || !**Address::Get()->s_mVehiclePool) return false;
	
	__try {
		
		return true;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

static bool Safe_ProcessPedPool()
{
	if (!Address::Get()->s_mPedPool || !*Address::Get()->s_mPedPool) return false;

	__try {
		(void)**Address::Get()->s_mPedPool;
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void Menu::RecoverImGuiStack()
{
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (!ctx) return;

	ImGuiContext& g = *ctx;
	while (g.GroupStack.Size > 0)
		ImGui::EndGroup();
	while (g.CurrentWindowStack.Size > 1) {
		ImGuiWindow* window = g.CurrentWindow;
		if (window && (window->Flags & ImGuiWindowFlags_ChildWindow))
			ImGui::EndChild();
		else
			ImGui::End();
	}
}

static bool Safe_ReadVehicleModelInfo(uint64_t vehAddr, unsigned int* hash)
{
	if (!vehAddr || !hash) return false;
	
	__try {
		uint64_t model_info = *(uint64_t*)(vehAddr + 0x20);
		if (model_info) *hash = *(uint32_t*)(model_info + 0x18);
		return true;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}



static bool FastRead3Floats_NoQV(uint64_t address, float* outX, float* outY, float* outZ)
{
	if (!outX || !outY || !outZ) return false;
	__try {
		const float* src = (const float*)address;
		*outX = src[0];
		*outY = src[1];
		*outZ = src[2];
		return true;
	} __except(EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

static bool GetEntityCoordsUltraFast(uint64_t entity, float* outX, float* outY, float* outZ)
{
	if (!entity || !outX || !outY || !outZ) return false;
	uint64_t nav = 0;
	__try { nav = *(uint64_t*)(entity + 0x30); } __except(EXCEPTION_EXECUTE_HANDLER) { nav = 0; }
	if (nav && FastRead3Floats_NoQV(nav + 0x50, outX, outY, outZ)) return true;
	return FastRead3Floats_NoQV(entity + 0x90, outX, outY, outZ);
}

static void BuildPlayerCacheImpl(
	std::vector<std::pair<void*, int>>& cachedPlayers,
	std::vector<std::string>& cachedNames,
	bool have_local,
	float lpx,
	float lpy,
	float lpz)
{
	for (auto cPed : **Address::Get()->s_mPedPool) {
		if (!cPed) continue;

		uint32_t ped_type = 0;
		if (!TryRead<uint32_t>((uint64_t)cPed + Address::Get()->m_pEntityType, &ped_type) || !ped_type) continue;
		ped_type = ped_type << 11 >> 25;
		if (ped_type != 2) continue;

		std::string nameStr;
		uint64_t playerinfo = 0;
		if (TryRead<uint64_t>((uint64_t)cPed + Address::Get()->m_pPlayerInfo, &playerinfo) && playerinfo) {
			nameStr = Address::Get()->GetPlayerNameFromInfo(playerinfo);
			if (nameStr.empty() || nameStr == "NPC") {
				uint64_t netid = 0;
				if (TryRead<uint64_t>(playerinfo + Address::Get()->m_pNetid, &netid)) {
					std::string name = Address::Get()->GetPlayerNameByNetId(netid);
					if (name != "NPC" && !name.empty()) nameStr = name;
				}
			}
		}
		if (nameStr.empty()) nameStr = "Player";

		int dist_m = 0;
		if (have_local) {
			float tx = 0.f, ty = 0.f, tz = 0.f;
			if (GetEntityCoordsFast((uint64_t)cPed, &tx, &ty, &tz)) {
				float dx = lpx - tx, dy = lpy - ty, dz = lpz - tz;
				dist_m = (int)(sqrtf(dx * dx + dy * dy + dz * dz));
			}
		}

		cachedPlayers.emplace_back((void*)cPed, dist_m);
		cachedNames.emplace_back(std::move(nameStr));
	}
}

static bool BuildPlayerCacheSEH(
	std::vector<std::pair<void*, int>>& cachedPlayers,
	std::vector<std::string>& cachedNames,
	bool have_local,
	float lpx,
	float lpy,
	float lpz)
{
	__try {
		BuildPlayerCacheImpl(cachedPlayers, cachedNames, have_local, lpx, lpy, lpz);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

Menu* Menu::s_pSingleton = nullptr;

Menu* Menu::Get() {
    if (!s_pSingleton) {
        s_pSingleton = new Menu();
    }
    return s_pSingleton;
}

char filterText[128];
static struct VehicleL {
	std::string name;
	unsigned int hash;
};
typedef std::unordered_map < unsigned int, std::string > VehicleMap;
std::vector<VehicleL> ServerVehicleList{

};


const std::string& LookupVehNameByHash(unsigned int hash) {
	static std::unordered_map<unsigned int, std::string> s_hashToName;
	if (s_hashToName.empty()) {
		auto add = [&](const char* code, const char* nice) {
			s_hashToName[rage::joaat(code)] = nice;
			};

		
		add("alpha", "Alpha");
		add("banshee", "Banshee");
		add("banshee2", "Banshee 900R");
		add("bestiagts", "Bestia GTS");
		add("blista2", "Blista Compact");
		add("blista3", "Blista Go Go Monkey");
		add("buffalo", "Buffalo");
		add("buffalo2", "Buffalo S");
		add("buffalo3", "Buffalo STX");
		add("calico", "Calico GTF");
		add("carbonizzare", "Carbonizzare");
		add("comet2", "Comet");
		add("comet3", "Comet Retro Custom");
		add("comet4", "Comet Safari");
		add("comet5", "Comet SR");
		add("comet6", "Comet S2");
		add("comet7", "Comet S2 Cabrio");
		add("coquette", "Coquette");
		add("coquette4", "Coquette D10");
		add("cypher", "Cypher");
		add("drafter", "8F Drafter");
		add("deveste", "Deveste Eight");
		add("elegy", "Elegy RH8");
		add("elegy2", "Elegy Retro Custom");
		add("everon2", "Hotring Everon");
		add("feltzer2", "Feltzer");
		add("flashgt", "Flash GT");
		add("furoregt", "Furore GT");
		add("fusilade", "Fusilade");
		add("futo", "Futo");
		add("futo2", "Futo GTX");
		add("gb200", "GB200");
		add("growler", "Growler");
		add("hotring", "Hotring Sabre");
		add("imorgon", "Imorgon");
		add("issi7", "Issi Sport");
		add("italirsx", "Itali RSX");
		add("jester", "Jester");
		add("jester2", "Jester (Racecar)");
		add("jester3", "Jester Classic");
		add("jester4", "Jester RR");
		add("journey2", "Journey II");
		add("jugular", "Jugular");
		add("khamelion", "Khamelion");
		add("komoda", "Komoda");
		add("kuruma", "Kuruma");
		add("kuruma2", "Kuruma (Armored)");
		add("locust", "Locust");
		add("lynx", "Lynx");
		add("massacro", "Massacro");
		add("massacro2", "Massacro (Racecar)");
		add("neo", "Neo");
		add("neon", "Neon");
		add("ninef", "Ninef");
		add("ninef2", "Ninef Cabrio");
		add("omnis", "Omnis");
		add("omnis2", "Omnis e-GT");
		add("paragon", "Paragon");
		add("paragon2", "Paragon R (Armored)");
		add("pariah", "Pariah");
		add("penumbra", "Penumbra");
		add("penumbra2", "Penumbra FF");
		add("raiden", "Raiden");
		add("rapidgt", "Rapid GT");
		add("rapidgt2", "Rapid GT Convertible");
		add("raptor", "Vapid Raptor");
		add("remus", "Remus");
		add("revolter", "Revolter");
		add("ruston", "Ruston");
		add("schafter2", "Schafter");
		add("schafter3", "Schafter V12");
		add("schafter4", "Schafter LWB");
		add("schlagen", "Schlagen GT");
		add("schwarzer", "Schwarzer");
		add("sentinel3", "Sentinel Classic");
		add("seven70", "Seven-70");
		add("specter", "Specter");
		add("specter2", "Specter Custom");
		add("streiter", "Streiter");
		add("sugoi", "Sugoi");
		add("sultan", "Sultan");
		add("sultan2", "Sultan Classic");
		add("sultan3", "Sultan RS Classic");
		add("sultanrs", "Sultan RS");
		add("surano", "Surano");
		add("tampa2", "Drift Tampa");
		add("tropos", "Tropos Rallye");
		add("verlierer2", "Verlierer");
		add("vstr", "V-STR");
		add("zr350", "ZR350");
		add("zr380", "ZR380");
		add("zr3802", "ZR380 (Apocalypse)");
		add("zr3803", "ZR380 (Future Shock)");

		
		add("adder", "Adder");
		add("autarch", "Autarch");
		add("banshee2", "Banshee 900R");
		add("bullet", "Bullet");
		add("cheetah", "Cheetah");
		add("cyclone", "Cyclone");
		add("deveste", "Deveste Eight");
		add("emerus", "Emerus");
		add("entity2", "Entity XXR");
		add("entity3", "Entity MT");
		add("entityxf", "Entity XF");
		add("fmj", "FMJ");
		add("furia", "Furia");
		add("gp1", "GP1");
		add("infernus", "Infernus");
		add("italigtb", "Itali GTB");
		add("italigtb2", "Itali GTB Custom");
		add("krieger", "Krieger");
		add("le7b", "RE-7B");
		add("nero", "Nero");
		add("nero2", "Nero Custom");
		add("osiris", "Osiris");
		add("penetrator", "Penetrator");
		add("pfister811", "Pfister 811");
		add("prototipo", "X80 Proto");
		add("reaper", "Reaper");
		add("s80", "S80RR");
		add("sc1", "SC1");
		add("scramjet", "Scramjet");
		add("sheava", "ETR1");
		add("sultanrs", "Sultan RS");
		add("t20", "T20");
		add("taipan", "Taipan");
		add("tempesta", "Tempesta");
		add("tezeract", "Tezeract");
		add("thrax", "Thrax");
		add("tigon", "Tigon");
		add("turismo2", "Turismo Classic");
		add("turismor", "Turismo R");
		add("tyrant", "Tyrant");
		add("tyrus", "Tyrus");
		add("vacca", "Vacca");
		add("vagner", "Vagner");
		add("vigilante", "Vigilante");
		add("visione", "Visione");
		add("voltic", "Voltic");
		add("voltic2", "Rocket Voltic");
		add("xa21", "XA-21");
		add("zentorno", "Zentorno");
		add("zorrusso", "Zorrusso");

		
		add("akuma", "Akuma");
		add("avarus", "Avarus");
		add("bagger", "Bagger");
		add("bati", "Bati 801");
		add("bati2", "Bati 801RR");
		add("bf400", "BF400");
		add("carbonrs", "Carbon RS");
		add("chimera", "Chimera");
		add("cliffhanger", "Cliffhanger");
		add("daemon", "Daemon");
		add("daemon2", "Daemon Custom");
		add("defiler", "Defiler");
		add("deathbike", "Deathbike");
		add("deathbike2", "Deathbike (Apocalypse)");
		add("deathbike3", "Deathbike (Future Shock)");
		add("diablous", "Diablous");
		add("diablous2", "Diablous Custom");
		add("double", "Double-T");
		add("enduro", "Enduro");
		add("esskey", "Esskey");
		add("faggio", "Faggio");
		add("faggio2", "Faggio Sport");
		add("faggio3", "Faggio Mod");
		add("fcr", "FCR 1000");
		add("fcr2", "FCR 1000 Custom");
		add("gargoyle", "Gargoyle");
		add("hakuchou", "Hakuchou");
		add("hakuchou2", "Hakuchou Drag");
		add("hexer", "Hexer");
		add("innovation", "Innovation");
		add("lectro", "Lectro");
		add("manchez", "Manchez");
		add("manchez2", "Manchez Scout");
		add("manchez3", "Manchez C");
		add("nemesis", "Nemesis");
		add("nightblade", "Nightblade");
		add("oppressor", "Oppressor");
		add("oppressor2", "Oppressor Mk II");
		add("pcj", "PCJ-600");
		add("ratbike", "Rat Bike");
		add("ruffian", "Ruffian");
		add("rrocket", "Rampant Rocket");
		add("sanchez", "Sanchez");
		add("sanchez2", "Sanchez (Livery)");
		add("sanctus", "Sanctus");
		add("shotaro", "Shotaro");
		add("sovereign", "Sovereign");
		add("stryder", "Stryder");
		add("thrust", "Thrust");
		add("vader", "Vader");
		add("vindicator", "Vindicator");
		add("vortex", "Vortex");
		add("wolfsbane", "Wolfsbane");
		add("zombiea", "Zombie Bobber");
		add("zombieb", "Zombie Chopper");

		
		add("asea", "Asea");
		add("asea2", "Asea (Snow)");
		add("asterope", "Asterope");
		add("cog55", "Cognoscenti 55");
		add("cog552", "Cognoscenti 55 (Armored)");
		add("cognoscenti", "Cognoscenti");
		add("cognoscenti2", "Cognoscenti (Armored)");
		add("emperor", "Emperor");
		add("emperor2", "Emperor (Rusty)");
		add("emperor3", "Emperor (Snow)");
		add("fugitive", "Fugitive");
		add("glendale", "Glendale");
		add("glendale2", "Glendale Custom");
		add("ingot", "Ingot");
		add("intruder", "Intruder");
		add("limo2", "Turreted Limo");
		add("premier", "Premier");
		add("primo", "Primo");
		add("primo2", "Primo Custom");
		add("regina", "Regina");
		add("romero", "Romero Hearse");
		add("schafter2", "Schafter");
		add("schafter3", "Schafter V12");
		add("schafter4", "Schafter LWB");
		add("schafter5", "Schafter V12 (Armored)");
		add("schafter6", "Schafter LWB (Armored)");
		add("stafford", "Stafford");
		add("stanier", "Stanier");
		add("stratum", "Stratum");
		add("stretch", "Stretch");
		add("superd", "Super Diamond");
		add("surge", "Surge");
		add("tailgater", "Tailgater");
		add("tailgater2", "Tailgater S");
		add("warrener", "Warrener");
		add("washington", "Washington");

		
		add("cogcabrio", "Cognoscenti Cabrio");
		add("exemplar", "Exemplar");
		add("felon", "Felon");
		add("felon2", "Felon GT");
		add("jackal", "Jackal");
		add("oracle", "Oracle");
		add("oracle2", "Oracle XS");
		add("previon", "Previon");
		add("sentinel", "Sentinel");
		add("sentinel2", "Sentinel XS");
		add("windsor", "Windsor");
		add("windsor2", "Windsor Drop");
		add("zion", "Zion");
		add("zion2", "Zion Cabrio");

		
		add("blade", "Blade");
		add("broadway", "Broadway");
		add("buccaneer", "Buccaneer");
		add("buccaneer2", "Buccaneer Custom");
		add("chino", "Chino");
		add("chino2", "Chino Custom");
		add("clique", "Clique");
		add("coquette3", "Coquette BlackFin");
		add("deviant", "Deviant");
		add("dominator", "Dominator");
		add("dominator2", "Pisswasser Dominator");
		add("dominator3", "Dominator GTX");
		add("dominator4", "Dominator ASP");
		add("dominator5", "Dominator GTT");
		add("dominator6", "Dominator (Apocalypse)");
		add("dominator7", "Dominator (Future Shock)");
		add("dominator8", "Dominator (Nightmare)");
		add("dukes", "Dukes");
		add("dukes2", "Dukes (Beater)");
		add("dukes3", "Duke O'Death");
		add("faction", "Faction");
		add("faction2", "Faction Custom");
		add("faction3", "Faction Custom Donk");
		add("ellie", "Ellie");
		add("gauntlet", "Gauntlet");
		add("gauntlet2", "Redwood Gauntlet");
		add("gauntlet3", "Gauntlet Classic");
		add("gauntlet4", "Gauntlet Hellfire");
		add("gauntlet5", "Gauntlet Custom");
		add("hermes", "Hermes");
		add("hotknife", "Hotknife");
		add("hustler", "Hustler");
		add("impaler", "Impaler");
		add("impaler2", "Impaler (Apocalypse)");
		add("impaler3", "Impaler (Future Shock)");
		add("impaler4", "Impaler (Nightmare)");
		add("imperator", "Imperator (Apocalypse)");
		add("imperator2", "Imperator (Future Shock)");
		add("imperator3", "Imperator (Nightmare)");
		add("lurcher", "Lurcher");
		add("moonbeam", "Moonbeam");
		add("moonbeam2", "Moonbeam Custom");
		add("nightshade", "Nightshade");
		add("peyote2", "Peyote Gasser");
		add("phoenix", "Phoenix");
		add("picador", "Picador");
		add("ratloader", "Rat-Loader");
		add("ratloader2", "Rat-Truck");
		add("ruiner", "Ruiner");
		add("ruiner2", "Ruiner 2000");
		add("ruiner3", "Ruiner (Wrecked)");
		add("sabregt", "Sabre Turbo");
		add("sabregt2", "Sabre Turbo Custom");
		add("slamvan", "Slamvan");
		add("slamvan2", "Lost Slamvan");
		add("slamvan3", "Slamvan Custom");
		add("slamvan4", "Slamvan (Apocalypse)");
		add("slamvan5", "Slamvan (Future Shock)");
		add("slamvan6", "Slamvan (Nightmare)");
		add("stalion", "Stallion");
		add("stalion2", "Stallion (Burger Shot)");
		add("tampa", "Tampa");
		add("tampa3", "Drift Tampa");
		add("tulip", "Tulip");
		add("vamos", "Vamos");
		add("vigero", "Vigero");
		add("virgo", "Virgo");
		add("virgo2", "Virgo Classic Custom");
		add("virgo3", "Virgo Classic");
		add("voodoo", "Voodoo");
		add("voodoo2", "Voodoo Custom");
		add("yosemite", "Yosemite");
		add("yosemite2", "Yosemite Drift");
		add("yosemite3", "Yosemite Rancher");

		
		add("baller", "Baller");
		add("baller2", "Baller (Second Generation)");
		add("baller3", "Baller LE");
		add("baller4", "Baller LE LWB");
		add("baller5", "Baller LE (Armored)");
		add("baller6", "Baller LE LWB (Armored)");
		add("baller7", "Baller ST");
		add("bjxl", "BeeJay XL");
		add("cavalcade", "Cavalcade");
		add("cavalcade2", "Cavalcade (Second Generation)");
		add("contender", "Contender");
		add("dubsta", "Dubsta");
		add("dubsta2", "Dubsta (Luxury)");
		add("dubsta3", "Dubsta 6x6");
		add("fq2", "FQ 2");
		add("granger", "Granger");
		add("gresley", "Gresley");
		add("habanero", "Habanero");
		add("huntley", "Huntley S");
		add("landstalker", "Landstalker");
		add("landstalker2", "Landstalker XL");
		add("mesa", "Mesa");
		add("mesa2", "Mesa (Snow)");
		add("mesa3", "Mesa (Merryweather)");
		add("novak", "Novak");
		add("patriot", "Patriot");
		add("patriot2", "Patriot Stretch");
		add("patriot3", "Patriot Mil-Spec");
		add("radi", "Radius");
		add("rebla", "Rebla GTS");
		add("rocoto", "Rocoto");
		add("seminole", "Seminole");
		add("seminole2", "Seminole Frontier");
		add("serrano", "Serrano");
		add("squaddie", "Squaddie");
		add("toros", "Toros");
		add("xls", "XLS");
		add("xls2", "XLS (Armored)");

		
		add("bfinjection", "BF Injection");
		add("bifta", "Bifta");
		add("blazer", "Blazer");
		add("blazer2", "Blazer Lifeguard");
		add("blazer3", "Blazer Hot Rod");
		add("blazer4", "Blazer Sport");
		add("blazer5", "Blazer Aqua");
		add("bodhi2", "Bodhi");
		add("brawler", "Brawler");
		add("bruiser", "Bruiser (Apocalypse)");
		add("bruiser2", "Bruiser (Future Shock)");
		add("bruiser3", "Bruiser (Nightmare)");
		add("brutus", "Brutus (Apocalypse)");
		add("brutus2", "Brutus (Future Shock)");
		add("brutus3", "Brutus (Nightmare)");
		add("caracara", "Caracara");
		add("caracara2", "Caracara 4x4");
		add("dloader", "Duneloader");
		add("dubsta3", "Dubsta 6x6");
		add("dune", "Dune Buggy");
		add("dune2", "Space Docker");
		add("dune3", "Dune FAV");
		add("dune4", "Rampant Rocket");
		add("dune5", "Dune Custom");
		add("everon", "Everon");
		add("freecrawler", "Freecrawler");
		add("hellion", "Hellion");
		add("insurgent", "Insurgent");
		add("insurgent2", "Insurgent Pick-Up");
		add("insurgent3", "Insurgent Pick-Up Custom");
		add("kalahari", "Kalahari");
		add("kamacho", "Kamacho");
		add("marshall", "Marshall");
		add("menacer", "Menacer");
		add("mesa3", "Mesa (Merryweather)");
		add("monster", "Liberator (Monster Truck)");
		add("monster3", "Sasquatch (Apocalypse)");
		add("monster4", "Sasquatch (Future Shock)");
		add("monster5", "Sasquatch (Nightmare)");
		add("nightshark", "Nightshark");
		add("outlaw", "Outlaw");
		add("rancherxl", "Rancher XL");
		add("rancherxl2", "Rancher XL (Snow)");
		add("rebel", "Rusty Rebel");
		add("rebel2", "Rebel");
		add("riata", "Riata");
		add("sandking", "Sandking XL");
		add("sandking2", "Sandking SWB");
		add("technical", "Technical");
		add("technical2", "Technical Aqua");
		add("technical3", "Technical Custom");
		add("trophytruck", "Trophy Truck");
		add("trophytruck2", "Desert Raid");
		add("vagrant", "Vagrant");
		add("verus", "Verus");
		add("winky", "Winky");
		add("yosemite3", "Yosemite Rancher");
		add("zhaba", "Zhaba");

		
		add("bison", "Bison");
		add("bison2", "Bison (Mighty Bush)");
		add("bison3", "Bison (Junkle)");
		add("bobcatxl", "Bobcat XL");
		add("boxville", "Boxville");
		add("boxville2", "Boxville (Post OP)");
		add("boxville3", "Boxville (Humane Labs)");
		add("boxville4", "Boxville (GoPostal)");
		add("boxville5", "Boxville (Armored)");
		add("burrito", "Burrito");
		add("burrito2", "Burrito (Gang)");
		add("burrito3", "Burrito (Bugstars)");
		add("burrito4", "Burrito (Snow)");
		add("burrito5", "Burrito (Construction)");
		add("camper", "Camper");
		add("gangburrito", "Gang Burrito");
		add("gangburrito2", "Gang Burrito (Custom)");
		add("journey", "Journey");
		add("minivan", "Minivan");
		add("minivan2", "Minivan Custom");
		add("paradise", "Paradise");
		add("pony", "Pony");
		add("pony2", "Pony (Weed)");
		add("rumpo", "Rumpo");
		add("rumpo2", "Rumpo Custom");
		add("rumpo3", "Rumpo (Weaponized)");
		add("speedo", "Speedo");
		add("speedo2", "Speedo Custom");
		add("speedo4", "Speedo (Clown Van)");
		add("surfer", "Surfer");
		add("surfer2", "Surfer (Rusty)");
		add("taco", "Taco Van");
		add("youga", "Youga");
		add("youga2", "Youga Classic");
		add("youga3", "Youga Classic 4x4");
		add("youga4", "Youga Custom");

		
		add("ambulance", "Ambulance");
		add("fbi", "FIB");
		add("fbi2", "FIB SUV");
		add("firetruk", "Fire Truck");
		add("lguard", "LifeGuard");
		add("pbus", "Prison Bus");
		add("police", "Police Cruiser");
		add("police2", "Police Cruiser (Buffalo)");
		add("police3", "Police Interceptor");
		add("police4", "Unmarked Cruiser");
		add("policeb", "Police Bike");
		add("policeold1", "Police Rancher");
		add("policeold2", "Police Roadcruiser");
		add("policet", "Police Transporter");
		add("pranger", "Park Ranger");
		add("predator", "Police Predator");
		add("riot", "Riot Van");
		add("riot2", "Riot Van (Half-track)");
		add("sheriff", "Sheriff Cruiser");
		add("sheriff2", "Sheriff SUV");
		add("towtruck", "Tow Truck");
		add("towtruck2", "Tow Truck (Large)");

		
		add("bulldozer", "Bulldozer");
		add("cutter", "Cutter");
		add("dump", "Dump");
		add("flatbed", "Flatbed");
		add("guardian", "Guardian");
		add("handler", "Dock Handler");
		add("mixer", "Cement Mixer");
		add("mixer2", "Cement Mixer (Large)");
		add("rubble", "Rubble");
		add("tiptruck", "Tip Truck");
		add("tiptruck2", "Tip Truck (Large)");

		
		add("airtug", "Airtug");
		add("caddy", "Caddy");
		add("caddy2", "Caddy (Utility)");
		add("caddy3", "Caddy (Weaponized)");
		add("docktug", "Docktug");
		add("forklift", "Forklift");
		add("mower", "Mower");
		add("ripley", "Ripley");
		add("sadler", "Sadler");
		add("sadler2", "Sadler (Rusty)");
		add("scrap", "Scrap Truck");
		add("towtruck", "Tow Truck");
		add("towtruck2", "Tow Truck (Large)");
		add("tractor", "Tractor");
		add("tractor2", "Fieldmaster");
		add("tractor3", "Fieldmaster (Rusty)");
		add("trailerlogs", "Logs Trailer");
		add("trailers", "Trailer");
		add("trailers2", "Trailer (Small)");
		add("trailers3", "Trailer (Flatbed)");
		add("trailers4", "Trailer (Rake)");
		add("trash", "Trashmaster");
		add("trash2", "Trashmaster (Heist)");
		add("utillitruck", "Utility Truck");
		add("utillitruck2", "Utility Truck (Pick-Up)");
		add("utillitruck3", "Utility Truck (Contained)");

		
		add("benson", "Benson");
		add("biff", "Biff");
		add("cerberus", "Cerberus (Apocalypse)");
		add("cerberus2", "Cerberus (Future Shock)");
		add("cerberus3", "Cerberus (Nightmare)");
		add("hauler", "Hauler");
		add("hauler2", "Hauler Custom");
		add("mule", "Mule");
		add("mule2", "Mule (Windowless)");
		add("mule3", "Mule (Weaponized)");
		add("mule4", "Mule (Custom)");
		add("mule5", "Mule (Ramp)");
		add("packer", "Packer");
		add("phantom", "Phantom");
		add("phantom2", "Phantom Wedge");
		add("phantom3", "Phantom Custom");
		add("pounder", "Pounder");
		add("pounder2", "Pounder Custom");
		add("stockade", "Stockade");
		add("stockade3", "Stockade (Brute)");
		add("terbyte", "Terrorbyte");

		
		add("avisa", "Kraken Avisa");
		add("dinghy", "Dinghy");
		add("dinghy2", "Dinghy (Black)");
		add("dinghy3", "Dinghy (Heist)");
		add("dinghy4", "Dinghy (Weaponized)");
		add("dinghy5", "Patrol Boat");
		add("jetmax", "Jetmax");
		add("kosatka", "Kosatka (Sub)");
		add("longfin", "Longfin");
		add("marquis", "Marquis");
		add("seashark", "Seashark");
		add("seashark2", "Seashark Lifeguard");
		add("seashark3", "Seashark Yacht");
		add("speeder", "Speeder");
		add("speeder2", "Speeder (Black)");
		add("squalo", "Squalo");
		add("submersible", "Submersible");
		add("submersible2", "Kraken");
		add("suntrap", "Suntrap");
		add("toro", "Toro");
		add("toro2", "Toro (Yacht)");
		add("tropic", "Tropic");
		add("tropic2", "Tropic (Yacht)");
		add("tug", "Tug");

		
		add("akula", "Akula");
		add("annihilator", "Annihilator");
		add("annihilator2", "Annihilator Stealth");
		add("buzzard", "Buzzard");
		add("buzzard2", "Buzzard Attack Chopper");
		add("cargobob", "Cargobob");
		add("cargobob2", "Cargobob Jetsam");
		add("cargobob3", "Cargobob TPE");
		add("cargobob4", "Cargobob (Fort Zancudo)");
		add("frogger", "Frogger");
		add("frogger2", "Frogger (Trevor)");
		add("havok", "Havok");
		add("hunter", "FH-1 Hunter");
		add("maverick", "Maverick");
		add("polmav", "Police Maverick");
		add("savage", "Savage");
		add("seasparrow", "Sea Sparrow");
		add("seasparrow2", "Sparrow");
		add("seasparrow3", "Sparrow (Weaponized)");
		add("skylift", "Skylift");
		add("supervolito", "SuperVolito");
		add("supervolito2", "SuperVolito Carbon");
		add("swift", "Swift");
		add("swift2", "Swift Deluxe");
		add("valkyrie", "HVY Valkyrie");
		add("valkyrie2", "Valkyrie MOD.0");
		add("volatus", "Volatus");

		
		add("alkonost", "RO-86 Alkonost");
		add("alphaz1", "Alpha-Z1");
		add("avenger", "Avenger");
		add("avenger2", "Avenger (Custom)");
		add("besra", "Besra");
		add("blimp", "Blimp");
		add("blimp2", "Xero Blimp");
		add("blimp3", "Blimp (Atomic)");
		add("bombushka", "RM-10 Bombushka");
		add("cargoplane", "Cargo Plane");
		add("cuban800", "Cuban 800");
		add("dodo", "Dodo");
		add("duster", "Duster");
		add("howard", "Howard NX-25");
		add("hydra", "Hydra");
		add("jet", "Jet");
		add("lazer", "P-996 LAZER");
		add("luxor", "Luxor");
		add("luxor2", "Luxor Deluxe");
		add("mammatus", "Mammatus");
		add("microlight", "Ultralight");
		add("miljet", "Miljet");
		add("mogul", "Mogul");
		add("molotok", "V-65 Molotok");
		add("nimbus", "Nimbus");
		add("nokota", "P-45 Nokota");
		add("pyro", "Pyro");
		add("rogue", "Rogue");
		add("seabreeze", "Seabreeze");
		add("shamal", "Shamal");
		add("starling", "LF-22 Starling");
		add("strikeforce", "B-11 Strikeforce");
		add("stunt", "Mallard");
		add("titan", "Titan");
		add("tula", "Tula");
		add("velum", "Velum");
		add("velum2", "Velum 5-Seater");
		add("vestra", "Vestra");
		add("volatol", "Volatol");

		
		add("deluxo", "Deluxo");
		add("scramjet", "Scramjet");
		add("vigilante", "Vigilante");
		add("ruiner2", "Ruiner 2000");
		add("phantom2", "Phantom Wedge");
		add("wastelander", "Wastelander");
		add("thruster", "Thruster (Jetpack)");

		
		add("freight", "Freight Train");
		add("freightcar", "Freight Car");
		add("freightcont1", "Freight Container");
		add("freightcont2", "Freight Container 2");
		add("freightgrain", "Freight Grain");
		add("metrotrain", "Metro Train");
		add("tankercar", "Tanker Car");
		add("armytrailer", "Army Trailer");
		add("armytrailer2", "Army Trailer (Covered)");
		add("baletrailer", "Bale Trailer");
		add("boattrailer", "Boat Trailer");
		add("cablecar", "Cable Car");
		add("docktrailer", "Dock Trailer");
		add("graintrailer", "Grain Trailer");
		add("proptrailer", "Prop Trailer");
		add("raketrailer", "Rake Trailer");
		add("tr2", "Trailer (Large)");
		add("tr3", "Trailer (Flatbed)");
		add("tr4", "Trailer (Logger)");
		add("tvtrailer", "TV Trailer");
		add("tanker", "Tanker");
		add("tanker2", "Tanker (Rusty)");
		add("trailerlarge", "Mobile Operations Center");
		add("trailersmall", "Anti-Aircraft Trailer");
		add("trailersmall2", "Anti-Aircraft Trailer (Weaponized)");
		add("veto", "Veto Classic");
		add("veto2", "Veto Modern");

		
		add("formula", "PR4");
		add("formula2", "R88");
		add("openwheel1", "BR8");
		add("openwheel2", "DR1");

		
		add("bus", "Bus");
		add("taxi", "Taxi");
		add("tourbus", "Tour Bus");
		add("policeb", "Police Bike");
		
	}
	
	for (const auto& v : ServerVehicleList) {
		if (v.hash == hash) return v.name;
	}
	static const std::string s_unknown = "Unknown";
	auto it = s_hashToName.find(hash);
	return (it != s_hashToName.end()) ? it->second : s_unknown;
}

static void PopulateFallbackVehicleList() {
	struct Pair { const char* name; const char* code; } items[] = {
		
		{ "Adder", "adder" }, { "T20", "t20" }, { "Zentorno", "zentorno" },
		{ "Osiris", "osiris" }, { "Nero", "nero" }, { "Nero2", "nero2" },
		{ "Comet5", "comet5" }, { "Elegy RH8", "elegy" }, { "Pariah", "pariah" },
		{ "Entity XF", "entityxf" }, { "Entity XXR", "entity2" }, { "Visione", "visione" },
		{ "Reaper", "reaper" }, { "Cyclone", "cyclone" }, { "Tezeract", "tezeract" },
		{ "Deveste", "deveste" }, { "Emerus", "emerus" }, { "Krieger", "krieger" },
		{ "Itali GTB", "italigtb" }, { "Itali GTB2", "italigtb2" }, { "Vagner", "vagner" },
		{ "XA-21", "xa21" }, { "SC1", "sc1" }, { "Autarch", "autarch" },

		
		{ "Dominator", "dominator" }, { "Dominator GTX", "dominator3" }, { "Gauntlet", "gauntlet" },
		{ "Gauntlet Hellfire", "gauntlet4" }, { "Dukes", "dukes" }, { "Duke O'Death", "dukes2" },
		{ "Sabre Turbo", "sabregt" }, { "Sabre Turbo Custom", "sabregt2" }, { "Ruiner", "ruiner" },
		{ "Ruiner 2000", "ruiner2" }, { "Phoenix", "phoenix" }, { "Tampa", "tampa" },
		{ "Drift Tampa", "tampa2" }, { "Vigero", "vigero" }, { "Clique", "clique" },
		{ "Yosemite", "yosemite" }, { "Ellie", "ellie" }, { "Hermes", "hermes" },

		
		{ "Kuruma", "kuruma" }, { "Kuruma2", "kuruma2" }, { "Sultan RS", "sultanrs" },
		{ "Sultan", "sultan" }, { "Schafter V12", "schafter2" }, { "Schafter LWB", "schafter3" },
		{ "Cognoscenti", "cognoscenti" }, { "Cognoscenti 55", "cognoscenti2" }, { "Super Diamond", "superd" },
		{ "Washington", "washington" }, { "Tailgater", "tailgater" }, { "Asterope", "asterope" },
		{ "Intruder", "intruder" }, { "Premier", "premier" }, { "Stretch", "stretch" },
		{ "Windsor", "windsor" }, { "Windsor Drop", "windsor2" }, { "Stafford", "stafford" },

		
		{ "Dubsta", "dubsta" }, { "Dubsta2", "dubsta2" }, { "Dubsta3", "dubsta3" },
		{ "Baller", "baller" }, { "Baller2", "baller2" }, { "Baller LE", "baller3" },
		{ "Baller LE LWB", "baller4" }, { "Baller ST", "baller5" }, { "Baller ST-D", "baller6" },
		{ "XLS", "xls" }, { "XLS2", "xls2" }, { "Granger", "granger" },
		{ "Patriot", "patriot" }, { "Patriot Stretch", "patriot2" }, { "Contender", "contender" },
		{ "Toros", "toros" }, { "Novak", "novak" }, { "Seminole", "seminole" },
		{ "Hellion", "hellion" }, { "Caracara", "caracara" }, { "Caracara 4x4", "caracara2" },

		
		{ "Bison", "bison" }, { "Bobcat XL", "bobcatxl" }, { "Sadler", "sadler" },
		{ "Sandking XL", "sandking" }, { "Sandking SWB", "sandking2" }, { "Guardian", "guardian" },
		{ "Mule", "mule" }, { "Mule2", "mule2" }, { "Mule3", "mule3" },
		{ "Pounder", "pounder" }, { "Pounder2", "pounder2" }, { "Youga", "youga" },
		{ "Youga2", "youga2" }, { "Youga Classic", "youga3" }, { "Gang Burrito", "gburrito" },
		{ "Gang Burrito2", "gburrito2" }, { "Camper", "camper" },

		
		{ "Akuma", "akuma" }, { "Bati 801", "bati" }, { "Bati 801RR", "bati2" },
		{ "Hakuchou", "hakuchou" }, { "Hakuchou Drag", "hakuchou2" }, { "Double T", "double" },
		{ "BF400", "bf400" }, { "Ruffian", "ruffian" }, { "Sanchez", "sanchez" },
		{ "Sanchez2", "sanchez2" }, { "Manchez", "manchez" }, { "Faggio", "faggio" },
		{ "Faggio2", "faggio2" }, { "Faggio3", "faggio3" }, { "Shotaro", "shotaro" },
		{ "Oppressor", "oppressor" }, { "Oppressor MK2", "oppressor2" }, { "Deathbike", "deathbike" },

		
		{ "Police", "police" }, { "Police2", "police2" }, { "Police3", "police3" },
		{ "Police4", "police4" }, { "Sheriff", "sheriff" }, { "Sheriff2", "sheriff2" },
		{ "Ambulance", "ambulance" }, { "Fire Truck", "firetruk" }, { "FBI", "fbi" },
		{ "FBI2", "fbi2" }, { "Policeb", "policeb" }, { "Police Old", "policeold1" },
		{ "Police Old2", "policeold2" }, { "Prison Bus", "pbus" }, { "Riot", "riot" },
		{ "Riot2", "riot2" }, { "LSPD Cruiser", "police" }, { "Park Ranger", "pranger" },

		
		{ "Buffalo", "buffalo" }, { "Buffalo2", "buffalo2" }, { "Buffalo3", "buffalo3" },
		{ "Fugitive", "fugitive" }, { "Asea", "asea" }, { "Asea2", "asea2" },
		{ "Premier", "premier" }, { "Ingot", "ingot" }, { "Stratum", "stratum" },
		{ "Warrener", "warrener" }, { "Glendale", "glendale" }, { "Pigalle", "pigalle" },
		{ "Blista", "blista" }, { "Blista2", "blista2" }, { "Blista3", "blista3" },
		{ "Dilettante", "dilettante" }, { "Issi", "issi2" }, { "Issi3", "issi3" },
		{ "Panto", "panto" }, { "Prairie", "prairie" }, { "Rhapsody", "rhapsody" },

		
		{ "Caddy", "caddy" }, { "Caddy2", "caddy2" }, { "Caddy3", "caddy3" },
		{ "Bus", "bus" }, { "Taxi", "taxi" }, { "Tourbus", "tourbus" },
		{ "Rental Bus", "rentalbus" }, { "Trashmaster", "trash" }, { "Trashmaster2", "trash2" },
		{ "Dump", "dump" }, { "Mixer", "mixer" }, { "Mixer2", "mixer2" },
		{ "Dozer", "bulldozer" }, { "Forklift", "forklift" }, { "Tractor", "tractor" },
		{ "Tractor2", "tractor2" }, { "Tractor3", "tractor3" }, { "Scrap", "scrap" }
	};

	for (auto& it : items) {
		ServerVehicleList.push_back(VehicleL{ it.name, rage::joaat(it.code) });
	}
}

void PopulateServerVehicleList() {
	
	ServerVehicleList.clear();

	uintptr_t addr = (uint64_t)GetModuleHandleA(sk("extra-natives-five.dll"));
	if (!addr) { PopulateFallbackVehicleList(); return; } 

	VehicleMap* g_vehicles = reinterpret_cast<VehicleMap*>(addr + 0x178130);
	if (!g_vehicles) { PopulateFallbackVehicleList(); return; }

	
	MEMORY_BASIC_INFORMATION mbi{};
	if (!VirtualQuery((LPCVOID)g_vehicles, &mbi, sizeof(mbi))) { PopulateFallbackVehicleList(); return; }
	if (mbi.State != MEM_COMMIT) { PopulateFallbackVehicleList(); return; }
	DWORD protect = mbi.Protect & 0xFF;
	if (!(protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE)) { PopulateFallbackVehicleList(); return; }

	
	if (g_vehicles->empty()) { PopulateFallbackVehicleList(); return; }

	for (const auto& [key, value] : *g_vehicles) {
		ServerVehicleList.push_back(VehicleL{ value, key });
	}
}

static bool updatedVehList = false;


static char lua_script_buf[LUA_SCRIPT_BUF_SIZE] = "";
static char lua_console_buf[LUA_CONSOLE_BUF_SIZE] = "";
static int lua_selected_resource = 0;
static wchar_t lua_last_path[MAX_PATH] = L"";
static std::map<std::string, int> lua_exec_counts;
static char lua_res_filter[128] = "";

#include <unordered_map>
#include <algorithm>
#include <cctype>
static std::unordered_map<std::string, double> s_lastStopReq;
static std::unordered_map<std::string, double> s_lastStartReq;

static std::unordered_map<std::string, bool> s_userStopped;
static bool lua_console_autoscroll = true;
static ImFont* g_LuaMonoFont = nullptr;

static void AppendLuaLog(const char* msg) {
	
	double t = ImGui::GetTime();
	int h = (int)(t / 3600.0);
	int m = (int)fmod(t / 60.0, 60.0);
	int s = (int)fmod(t, 60.0);
	char line[512];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "[%02d:%02d:%02d] %s\n", h, m, s, msg);
	size_t cur = strlen(lua_console_buf);
	size_t rem = sizeof(lua_console_buf) - cur - 1;
	if (rem > 0) {
		_snprintf_s(lua_console_buf + cur, rem + 1, _TRUNCATE, "%s", line);
	}
}

static std::string ReadFileToString(const wchar_t* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return data;
}

static std::string ReadFileToStringA(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return data;
}

static uintmax_t GetFolderSizeNoThrow(const std::string& root) {
    namespace fs = std::filesystem;
    std::error_code ec;
    uintmax_t total = 0;
    fs::path p(root);
    if (!fs::exists(p, ec)) return 0;
    for (fs::recursive_directory_iterator it(p, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (it->is_regular_file(ec)) {
            total += it->file_size(ec);
        }
    }
    return total;
}

static const char* ClassifyResourceTypeFromManifest(const std::string& manifestContent) {
    if (manifestContent.find("this_is_a_map") != std::string::npos || manifestContent.find("map 'yes'") != std::string::npos)
        return "Map";
    if (manifestContent.find("client_script") != std::string::npos || manifestContent.find("server_script") != std::string::npos)
        return "Script";
    if (manifestContent.find("data_file") != std::string::npos || manifestContent.find("files") != std::string::npos)
        return "Asset";
    return "Unknown";
}

static std::string ExtractAuthorFromManifest(const std::string& manifestContent) {
    
    auto pos = manifestContent.find("author");
    if (pos == std::string::npos) return {};
    for (size_t i = pos; i < manifestContent.size(); ++i) {
        if (manifestContent[i] == '\'' || manifestContent[i] == '"') {
            char q = manifestContent[i];
            size_t j = manifestContent.find(q, i + 1);
            if (j != std::string::npos && j > i + 1) return manifestContent.substr(i + 1, j - i - 1);
            break;
        }
    }
    return {};
}

static std::string FormatBytes(uintmax_t bytes) {
    const double kb = 1024.0, mb = kb * 1024.0, gb = mb * 1024.0;
    char buf[64];
    if (bytes >= (uintmax_t)gb) { _snprintf_s(buf, _TRUNCATE, "%.2f GB", bytes / gb); }
    else if (bytes >= (uintmax_t)mb) { _snprintf_s(buf, _TRUNCATE, "%.2f MB", bytes / mb); }
    else if (bytes >= (uintmax_t)kb) { _snprintf_s(buf, _TRUNCATE, "%.2f KB", bytes / kb); }
    else { _snprintf_s(buf, _TRUNCATE, "%llu B", (unsigned long long)bytes); }
    return buf;
}

static bool OpenLuaFileDialog(std::wstring& outPath) {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"Lua Files (*.lua)\0*.lua\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"lua";
    if (GetOpenFileNameW(&ofn)) { outPath = fileName; return true; }
    return false;
}

static bool SaveLuaFileDialog(std::wstring& outPath)
{
    wchar_t fileName[MAX_PATH] = L"script.lua";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"Lua Files (*.lua)\0*.lua\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"lua";
    if (GetSaveFileNameW(&ofn)) { outPath = fileName; return true; }
    return false;
}

static bool WriteStringToFile(const wchar_t* path, const char* data, size_t len)
{
    if (!path || !data) return false;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(data, (std::streamsize)len);
    return file.good();
}

static void GetResourcesSafeImpl(std::vector<fx::fwRefContainer<fx::Resource>>& out)
{
    Resources::refresh();
    for (const auto& res : Resources::ResourceList) {
        if (res.Pointer) {
            fx::Resource* r = reinterpret_cast<fx::Resource*>(res.Pointer);
            out.emplace_back(r);
        }
    }
}

static bool GetResourcesSafeSEH(std::vector<fx::fwRefContainer<fx::Resource>>& out)
{
    __try {
        GetResourcesSafeImpl(out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static std::vector<fx::fwRefContainer<fx::Resource>> GetResourcesSafe() {
    std::vector<fx::fwRefContainer<fx::Resource>> out;
    if (!GetResourcesSafeSEH(out))
        out.clear();
    return out;
}


static std::vector<fx::fwRefContainer<fx::Resource>> s_cachedResources;
static double s_lastResRefresh = 0.0;
static const std::vector<fx::fwRefContainer<fx::Resource>>& GetResourcesCached()
{
    if ((ImGui::GetTime() - s_lastResRefresh) > 1.0) {
        s_cachedResources = GetResourcesSafe();
        s_lastResRefresh = ImGui::GetTime();
    }
    return s_cachedResources;
}

static bool HasSafeSessionContext()
{
    if (!Address::Get()->m_sPedFactory) return false;

    uint64_t world = 0;
    if (!TryRead<uint64_t>(Address::Get()->m_sPedFactory, &world) || !world)
        return false;

    uint64_t localPlayer = 0;
    if (!TryRead<uint64_t>(world + 0x8, &localPlayer) || !localPlayer)
        return false;

    float x = 0.f, y = 0.f, z = 0.f;
    if (!GetEntityCoordsFast(localPlayer, &x, &y, &z))
        return false;

    return Safe_ProcessPedPool();
}

static bool IsLuaTabAvailable()
{
    if (!HasSafeSessionContext())
        return false;

    auto resources = GetResourcesCached();
    if (!resources.empty()) return true;
    resources = GetResourcesSafe();
    return !resources.empty();
}

static bool s_luaTabConfirmed = false;

static bool EnsureLuaTabConfirmed(const char* childName)
{
    if (s_luaTabConfirmed && IsLuaTabAvailable())
        return true;

    s_luaTabConfirmed = false;

    ui::begin_child(childName, ImVec2(ImGui::GetContentRegionAvail().x, 0)); {
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::TextWrapped("The Lua tab is sensitive and should only be used while connected to a server.");
        ImGui::Dummy(ImVec2(0, 6));

        if (HasSafeSessionContext())
            ImGui::TextDisabled("Server session detected. Click confirm to open the Lua tab.");
        else
            ImGui::TextDisabled("No server session detected at the moment.");

        ImGui::Dummy(ImVec2(0, 10));
        if (ui::modern_button(sk("Confirm"), ImVec2(180, 0))) {
            if (IsLuaTabAvailable()) {
                s_luaTabConfirmed = true;
                AppendLuaLog("Lua access confirmed for current session.");
            } else {
                AppendLuaLog("Lua confirmation denied: no active server session.");
            }
        }
    } ui::end_child();

    return s_luaTabConfirmed;
}


static const char* kSafeResAllowlist[] = {
    "monitor",
    "chat",
    "sessionmanager",
    "spawnmanager",
    "baseevents",
    "hardcap",
    "mapmanager",
    "playernames",
    "rconlog",
    "scoreboard",
    "basic-gamemode",
    "citizen-scripting-core",
    "citizen-scripting-lua",
    "fivem",
};


static const char* kSafeResPrefixes[] = {
    "monitor",
    "chat",
    "session",
    "spawn",
    "baseevent",
    "hardcap",
    "mapmanager",
    "playername",
    "rcon",
    "scoreboard",
    "basic",
    "citizen",
    "fivem",
};

static bool equals_icase(const std::string& a, const char* b)
{
    if (!b) return false;
    if (a.size() != strlen(b)) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = (char)tolower(a[i]);
        char cb = (char)tolower(b[i]);
        if (ca != cb) return false;
    }
    return true;
}


static bool IsLikelyUnsafeResourceName(const std::string& name)
{
    std::string n;
    n.resize(name.size());
    std::transform(name.begin(), name.end(), n.begin(), [](unsigned char c){ return (char)tolower(c); });
    const char* bads[] = { "esx", "qb-", "qb_", "vrp", "ox_", "ox-", "inventory", "anticheat", "ac", "jobs", "phone", "custom" };
    for (auto* b : bads) {
        if (n.find(b) != std::string::npos) return true;
    }
    return false;
}

static fx::fwRefContainer<fx::Resource> PickSafeResource()
{
    auto list = GetResourcesCached();
    if (list.empty()) list = GetResourcesSafe();
    
    for (auto& r : list) {
        if (!r.GetRef()) continue;
        std::string nm = r->get_impl()->GetName();
        for (auto* allowed : kSafeResAllowlist) {
            if (equals_icase(nm, allowed)) return r;
        }
    }
    
    for (auto& r : list) {
        if (!r.GetRef()) continue;
        std::string nm = r->get_impl()->GetName();
        if (nm.empty()) continue;
        std::string low = nm;
        std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c){ return (char)tolower(c); });
        for (auto* pref : kSafeResPrefixes) {
            if (low.find(pref) != std::string::npos) return r;
        }
    }
    
    return fx::fwRefContainer<fx::Resource>();
}


static inline void SafeLuaEnqueueScript(const std::string& script);
static void TickLuaInjector();
static bool SafeLuaExecOnResourceChunked(const fx::fwRefContainer<fx::Resource>& r, const std::string& script);

static inline std::string WrapLuaScriptForSafeExec(const std::string& body)
{
    
    std::string wrapped;
    wrapped.reserve(body.size() + 128);
    wrapped += "Citizen.CreateThread(function() Wait(0) local ok,err=pcall(function() ";
    wrapped += body;
    wrapped += " end) if not ok then print('[Pink][Lua error]: '..tostring(err)) end end)";
    return wrapped;
}


static inline std::string Base64Encode(const uint8_t* data, size_t len)
{
    static const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len * 4) / 3 + 4);
    size_t i = 0;
    while (i + 2 < len) {
        uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | uint32_t(data[i + 2]);
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back(kB64[(n >> 6) & 63]);
        out.push_back(kB64[n & 63]);
        i += 3;
    }
    if (i < len) {
        uint32_t n = uint32_t(data[i]) << 16;
        out.push_back(kB64[(n >> 18) & 63]);
        if (i + 1 < len) {
            n |= (uint32_t(data[i + 1]) << 8);
            out.push_back(kB64[(n >> 12) & 63]);
            out.push_back(kB64[(n >> 6) & 63]);
            out.push_back('=');
        } else {
            out.push_back(kB64[(n >> 12) & 63]);
            out.push_back('=');
            out.push_back('=');
        }
    }
    return out;
}


static inline std::string BuildB64ExecPayload(const std::string& script)
{
    std::string b64 = Base64Encode(reinterpret_cast<const uint8_t*>(script.data()), script.size());
    std::string p;
    p.reserve(b64.size() + 512);
    p += "local __b='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'\n";
    p += "local function __d(data) data=data:gsub('[^'..__b..'=]',''); return (data:gsub('.', function(x) if x=='=' then return '' end local r,f='',(__b:find(x,1,true)-1) for i=6,1,-1 do r=r..(f%2^i - f%2^(i-1) > 0 and '1' or '0') end return r end):gsub('%d%d%d?%d?%d?%d?%d?%d?', function(x) if #x~=8 then return '' end local c=0 for i=1,8 do c=c + (x:sub(i,i)=='1' and 2^(8-i) or 0) end return string.char(c) end)) end\n";
    p += "do local _c=[[";
    p += b64;
    p += "]]; local _s=__d(_c); _c=nil; Citizen.CreateThread(function() local ok,err=pcall(function() local fn,e=load(_s); if not fn then error(e) end; fn() end); if not ok then print('[Pink][Lua error]: '..tostring(err)) end end) end";
    return p;
}


static inline std::string LuaEscapeForSingleQuoted(const std::string& src)
{
    std::string out;
    out.reserve(src.size() * 2);
    for (char c : src) {
        switch (c) {
        case '\\': out += "\\\\"; break;  
        case '\'': out += "\\'"; break;     
        case '\n': out += "\\n"; break;     
        case '\r': out += "\\r"; break;     
        case '\t': out += "\\t"; break;     
        default: out += c; break;
        }
    }
    return out;
}



static void BuildLuaChunkFragments(const std::string& script, size_t chunkSize, std::vector<std::string>& outFragments)
{
    const std::string escaped = LuaEscapeForSingleQuoted(script);
    const size_t total = escaped.size();
    size_t pos = 0;

    
    outFragments.emplace_back("_PINKC = (_PINKC or '')\n");

    while (pos < total) {
        const size_t len = (total - pos > chunkSize) ? chunkSize : (total - pos);
        std::string stmt;
        stmt.reserve(len + 64);
        stmt += "_PINKC = _PINKC .. '";
        stmt.append(escaped.c_str() + pos, len);
        stmt += "'\n";
        outFragments.emplace_back(std::move(stmt));
        pos += len;
    }

    
    outFragments.emplace_back(
        "do local _src=_PINKC; _PINKC=nil; Citizen.CreateThread(function() "
        "local ok,err=pcall(function() local fn,e=load(_src); if not fn then error(e) end; fn() end); "
        "if not ok then print('[Pink][Lua error]: '..tostring(err)) end end) end\n");
}


static std::string BuildSingleMessageChunkedPayload(const std::string& script, size_t chunkSize)
{
    const std::string escaped = LuaEscapeForSingleQuoted(script);
    const size_t total = escaped.size();
    size_t pos = 0;
    std::string payload;
    
    payload.reserve(total + 256);
    payload += "local _t={}; ";
    while (pos < total) {
        const size_t len = (total - pos > chunkSize) ? chunkSize : (total - pos);
        payload += "_t[#_t+1]='";
        payload.append(escaped.c_str() + pos, len);
        payload += "'; ";
        pos += len;
    }
    payload += "local _src=table.concat(_t); _t=nil; "
               "Citizen.CreateThread(function() "
               "local ok,err=pcall(function() local fn,e=load(_src); if not fn then error(e) end; fn() end); "
               "if not ok then print('[Pink][Lua error]: '..tostring(err)) end end)";
    return payload;
}


static bool SafeLuaExecOnResourceChunked(const fx::fwRefContainer<fx::Resource>& r, const std::string& script)
{
    if (!r.GetRef()) return false;
    std::string resName = r->get_impl()->GetName();
    if (resName.empty()) return false;

    
    std::string payload = BuildB64ExecPayload(script);

    char dbg[256];
    _snprintf_s(dbg, _countof(dbg), _TRUNCATE, "Chunked(exec one-shot) via '%s': payload %zu bytes", resName.c_str(), payload.size());
    AppendLuaLog(dbg);

    
    lua_exec_counts[resName] = 0;
    auto inserted = std::make_shared<bool>(false);
    size_t connHandle = r->Runtime.Connect([resName, payload, inserted](std::vector<char>* info){
        int& count = lua_exec_counts[resName];
        if (!*inserted && (count == 4 || count == 6)) {
            info->insert(info->begin(), payload.begin(), payload.end());
            *inserted = true;
        }
        count++;
    });

    
    r->Stop();
    r->Start();
    r->Runtime.Disconnect(connHandle);

    char logbuf[256];
    _snprintf_s(logbuf, _countof(logbuf), _TRUNCATE, "Executed (chunked one-shot) via '%s'", resName.c_str());
    AppendLuaLog(logbuf);
    return *inserted;
}

static bool SafeLuaExecOnResource(const fx::fwRefContainer<fx::Resource>& r, const std::string& script)
{
    if (!r.GetRef()) return false;
    std::string resName = r->get_impl()->GetName();
    if (resName.empty()) return false;
    
    if (script.size() > (128 * 1024)) {
        AppendLuaLog("Script too large (>128KB) — aborted for safety.");
        return false;
    }
    
    
    char size_msg[128];
    _snprintf_s(size_msg, _countof(size_msg), _TRUNCATE, "Script size: %zu bytes", script.size());
    AppendLuaLog(size_msg);

    lua_exec_counts[resName] = 0;
    std::string payload = WrapLuaScriptForSafeExec(script) + ";";
    auto inserted = std::make_shared<bool>(false);

    char debug_msg[256];
    _snprintf_s(debug_msg, _countof(debug_msg), _TRUNCATE, "Injecting to '%s': payload %zu chars", resName.c_str(), payload.size());
    AppendLuaLog(debug_msg);

    size_t connHandle = r->Runtime.Connect([resName, payload, inserted](std::vector<char>* info){
        int& count = lua_exec_counts[resName];
        if (!*inserted && (count == 4 || count == 6)) {
            info->insert(info->begin(), payload.begin(), payload.end());
            *inserted = true;
        }
        count++;
    });

    
    r->Stop();
    r->Start();
    r->Runtime.Disconnect(connHandle);

    char logbuf[256];
    _snprintf_s(logbuf, _countof(logbuf), _TRUNCATE, "Executed via '%s' (inserted=%d)", resName.c_str(), *inserted ? 1 : 0);
    AppendLuaLog(logbuf);
    return *inserted;
}

static bool SafeLuaExecScript(const std::string& script)
{
    auto r = PickSafeResource();
    if (!r.GetRef()) {
        AppendLuaLog("No safe resource available for Lua execution.");
        return false;
    }
    AppendLuaLog("Found safe resource for execution");
    
    if (script.size() > 1500) {
        return SafeLuaExecOnResourceChunked(r, script);
    }
    return SafeLuaExecOnResource(r, script);
}

 
 static bool SpawnPropViaLuaAt(const char* propModelCode, float x, float y, float z)
 {
     
     char script[512];
     _snprintf_s(script, _countof(script), _TRUNCATE,
         "local m=GetHashKey(\"%s\"); "
         "RequestModel(m); while not HasModelLoaded(m) do Wait(0) end; "
         
         "local obj=CreateObject(m, %.3f, %.3f, %.3f, false, true, false); "
         "SetEntityAsMissionEntity(obj, true, true); "
         "SetEntityCollision(obj, true, true); "
         "FreezeEntityPosition(obj, true); "
         "SetModelAsNoLongerNeeded(m);",
         propModelCode, x, y, z);
     return SafeLuaExecScript(std::string(script));
 }

 
 static bool SpawnVehicleViaLuaHash(unsigned int modelHash)
 {
     char script[768];
     _snprintf_s(script, _countof(script), _TRUNCATE,
         "local m=%u; "
         "RequestModel(m); while not HasModelLoaded(m) do Wait(0) end; "
         "local ped=PlayerPedId(); local px,py,pz=table.unpack(GetEntityCoords(ped)); "
         "local h=GetEntityHeading(ped); local rad=h*0.01745329252; "
         "local sx=px + math.sin(rad)*4.0; local sy=py + math.cos(rad)*4.0; local sz=pz + 0.6; "
         "local veh=CreateVehicle(m, sx, sy, sz, h, true, true); "
         "SetVehicleOnGroundProperly(veh); SetEntityAsMissionEntity(veh, true, true); SetModelAsNoLongerNeeded(m);",
         modelHash);

     return SafeLuaExecScript(std::string(script));
 }

 
 static bool SpawnVehicleImmediate(unsigned int modelHash)
 {
     if (!MemoryAddress::create_vehicle) return false;

     
     uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
     uint64_t lp = world ? *(uint64_t*)(world + 0x8) : 0;
     if (!lp) return false;

     
     DWORD64 cam = Core::Get()->GetCamera();
     Vector3 forward = cam ? *(Vector3*)(cam + 0x3D0) : Vector3(0.f, 1.f, 0.f);
     if (forward.is_zero()) forward = cam ? *(Vector3*)(cam + 0x40) : Vector3(0.f, 1.f, 0.f);
     if (!forward.is_zero()) forward = forward.normalize();
     Vector3 base = *(Vector3*)(lp + 0x90);
     Vector3 spawn = base; spawn.x += forward.x * 4.0f; spawn.y += forward.y * 4.0f; spawn.z += 1.2f;
     float heading = (!forward.is_zero()) ? atan2f(forward.x, forward.y) * 57.2957795f : 0.0f;

     
     if (MemoryAddress::request_model) MemoryAddress::request_model(modelHash);
     if (MemoryAddress::has_model_loaded) {
         for (int i = 0; i < 250; ++i) { 
             if (MemoryAddress::has_model_loaded(modelHash)) break;
             Sleep(1);
         }
     }

     PVector3 pos(spawn.x, spawn.y, spawn.z);
     uint64_t veh_ptr = MemoryAddress::create_vehicle(modelHash, pos, heading, true, true);
     if (!veh_ptr) veh_ptr = MemoryAddress::create_vehicle(modelHash, pos, heading, false, false);
     if (veh_ptr && MemoryAddress::Set_forward_speed) MemoryAddress::Set_forward_speed(veh_ptr, 0.0f);
     if (MemoryAddress::set_model_no_longer_needed) MemoryAddress::set_model_no_longer_needed(modelHash);
     return veh_ptr != 0;
 }

 
 static void ExecuteReviveLua(const std::string& script)
 {
    SafeLuaEnqueueScript(script);
 }

 static void CrashPlayerViaLua(uint64_t targetPedPtr)
 {
	 if (!targetPedPtr || !MemoryAddress::pointer_to_handle) return;

	 int targetHandle = MemoryAddress::pointer_to_handle((intptr_t)targetPedPtr);
	 if (!targetHandle) return;

	 char script[2048];
	 _snprintf_s(script, _countof(script), _TRUNCATE,
		 "Citizen.CreateThread(function() "
		 "  local target = %d; "
		 "  local m = GetHashKey('ig_wade'); "
		 "  RequestModel(m); "
		 "  local start = GetGameTimer(); "
		 "  while not HasModelLoaded(m) and (GetGameTimer() - start < 3000) do Wait(0) end; "
		 "  if not HasModelLoaded(m) then return end; "
		 "  for i = 0, 32 do "
		 "    local coords = GetEntityCoords(target); "
		 "    local ped = CreatePed(21, m, coords.x, coords.y, coords.z, 0.0, true, false); "
		 "    if DoesEntityExist(ped) then "
		 "      SetEntityVisible(ped, false, false); "
		 "      GiveWeaponToPed(ped, GetHashKey('WEAPON_RPG'), 9999, true, true); "
		 "      SetPedCanSwitchWeapon(ped, true); "
		 "      TaskCombatPed(ped, target, 0, 16); "
		 "    end; "
		 "    Wait(50); "
		 "  end; "
		 "  SetModelAsNoLongerNeeded(m); "
		 "end)",
		 targetHandle);

	 SafeLuaExecScript(std::string(script));
 }

 void Menu::LoadImGui()
 {
 	
     if (lua_script_buf[0] == '\0') {
         const char* def = "print(\"Hellow wold\")";
         strncpy_s(lua_script_buf, def, _TRUNCATE);
     }
 	if (ImGui::GetCurrentContext() == nullptr)
 		ImGui::CreateContext();

 	ui::styles();
 	ui::colors();

 	ImGuiIO& io = ImGui::GetIO();
 	
 	io.ConfigFlags = ImGuiConfigFlags_None;
 	io.IniFilename = nullptr;
 	io.LogFilename = nullptr;
 	ImFontConfig font_config;
 	font_config.PixelSnapH = false;
 	font_config.FontDataOwnedByAtlas = false;
 	font_config.OversampleH = 5;
 	font_config.OversampleV = 5;
 	font_config.RasterizerMultiply = 1.2f;

 	static const ImWchar ranges[] =
 	{
 		0x0020, 0x00FF, 
 		0x0400, 0x052F, 
 		0x2DE0, 0x2DFF, 
 		0xA640, 0xA69F, 
 		0xE000, 0xE226, 
 		0,
 	};

 	static bool s_fonts_loaded = false;
 	if (!s_fonts_loaded) {
 		fonts[font].set_data(b_font, sizeof(b_font));
 		fonts[fontb].set_data(b_fontb, sizeof(b_fontb));
 		fonts[icons].set_data(glyphter, sizeof(glyphter));

 		fonts[font].init(16);
 		fonts[fontb].init(12);
 		fonts[icons].init(16);

 		
 		static ImFont* s_fontVerdana = nullptr;
 		if (!s_fontVerdana) {
 			s_fontVerdana = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf", 13.0f, &font_config, io.Fonts->GetGlyphRangesDefault());
 			if (s_fontVerdana) {
 				io.FontDefault = s_fontVerdana;
 			}
 		}

 		
 		if (g_TitleFont == nullptr)
 			g_TitleFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 56.0f, &font_config, io.Fonts->GetGlyphRangesDefault());
 		if (g_SubTitleFont == nullptr)
 			g_SubTitleFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 36.0f, &font_config, io.Fonts->GetGlyphRangesDefault());

 		
 		if (!g_LuaMonoFont)
 			g_LuaMonoFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, &font_config, io.Fonts->GetGlyphRangesDefault());

 		s_fonts_loaded = true;
 	}


 	
 	{
 		ImGuiStyle& s = ImGui::GetStyle();
 		s.Colors[ImGuiCol_FrameBg]        = ImVec4(0.04f, 0.04f, 0.04f, 0.82f);
 		s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.06f, 0.06f, 0.06f, 0.90f);
 		s.Colors[ImGuiCol_FrameBgActive]  = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
 		s.Colors[ImGuiCol_PopupBg]        = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
 	}

 	ui::add_page(0, []() {
 		ImGui::BeginGroup();
 		{
 			ui::begin_child(sk("Global")); {

 				ImGui::Checkbox(sk("Aimbot Activate"), &globals.aimbot.active);
 				ImGui::Checkbox(sk("Silent Activate"), &globals.aimbot.silent_aim);
 				if (globals.aimbot.silent_aim) {

 					ImGui::SameLine();
 					ImGui::ColorEdit4(sk("Silent Fov Col"), (float*)&globals.visuals.silent_fov_color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoBorder);
 					ImGui::Checkbox(sk("Magic Bullet"), &globals.aimbot.magic_aim);
 				}
 				ImGui::Checkbox(sk("Show Fov"), &globals.aimbot.draw_fov);

 				
 				ImGui::Checkbox(sk("Closest Bone"), &globals.aimbot.closest_bone);
 				ImGui::Checkbox(sk("Draw Aim Line"), &globals.aimbot.draw_aim_line);
 				
 				
 				

 			} ui::end_child();

 			ui::begin_child(sk("Filters")); {

 				ImGui::Checkbox(sk("Point Players"), &globals.aimbot.point_players);
 				ImGui::Checkbox(sk("Point NPCs"), &globals.aimbot.point_npcs);
 				ImGui::Checkbox(sk("Point Dead"), &globals.aimbot.point_dead);
 				ImGui::Checkbox(sk("Point Animals"), &globals.aimbot.point_animals);

 			} ui::end_child();
 		}
 		ImGui::EndGroup();

 		ImGui::SameLine();

 		ImGui::BeginGroup(); {

 			ui::begin_child(sk("Settings")); {

 				ui::slider_int(sk("Aimbot Fov Size"), &globals.aimbot.fov, 0, 180, "%d", ImGuiSliderFlags_None);
 				
 				ui::slider_int(sk("Silent Fov Size"), &globals.aimbot.silent_fov, 0, 180, "%d", ImGuiSliderFlags_None);
 				ui::thin_slider_int(sk("Miss Chance %"), &globals.aimbot.silent_miss_chance, 0, 100, "%d");
 				ui::slider_int(sk("Smooth"), &globals.aimbot.smooth, 0, 100, "%d", ImGuiSliderFlags_None);
 				ui::slider_int(sk("Max Distance"), &globals.aimbot.max_dist, 0, 500, "%d", ImGuiSliderFlags_None);
 				ui::binder(sk("Aim Key"), &globals.aimbot.hotkey);
 				ui::binder(sk("Silent Aim Key"), &globals.aimbot.silent_hotkey);
 				ImGui::Combo(sk("Target Bone"), &globals.aimbot.bone, globals.aimbot.bones_list);
 				ImGui::Combo(sk("Target Priority"), &globals.aimbot.target_priority, globals.aimbot.aim_priorities);

 			} ui::end_child();

 			
 		}
 		ImGui::EndGroup();
 		});

 		

 	ui::add_page(1, []() {
 		ImGui::BeginGroup();
 		{
 			ui::begin_child(sk("Player Visuals")); {

 				ImGui::Checkbox(sk("Activate"), &globals.visuals.activate);
 				ImGui::Checkbox(sk("Box"), &globals.visuals.box, 0, globals.visuals.boxcolor);
 				ImGui::Checkbox(sk("Skeleton"), &globals.visuals.skeleton, 0, globals.visuals.skeleton_color);
 				ImGui::Checkbox(sk("Health Bar"), &globals.visuals.healthbar);
 				ImGui::Checkbox(sk("Armor Bar"), &globals.visuals.armorbar);
 				ImGui::Checkbox(sk("Names"), &globals.visuals.names, 0, globals.visuals.namecolor);
 				ImGui::Checkbox(sk("Weapon"), &globals.visuals.weapon_name, 0, globals.visuals.weapon_color);
 				ImGui::Checkbox(sk("Distance"), &globals.visuals.distance, 0, globals.visuals.distance_color);
 				ImGui::Checkbox(sk("Traces"), &globals.visuals.snapline, 0, globals.visuals.snapline_color);

 			} ui::end_child();

 			ui::begin_child(sk("Filters")); {

 				ImGui::Checkbox(sk("Show Ped"), &globals.visuals.show_npcs);
 				ImGui::Checkbox(sk("Show Self"), &globals.visuals.show_self);
 				ImGui::Checkbox(sk("Show Dead"), &globals.visuals.show_dead);
 				ImGui::Checkbox(sk("Show Animals"), &globals.visuals.show_animals);
 				

 			} ui::end_child();
 		}
 		ImGui::EndGroup();

 		ImGui::SameLine();

 		ImGui::BeginGroup(); {
 			ui::begin_child(sk("Configuration")); {

 				ui::slider_int(sk("View Distance"), &globals.visuals.view_distance, 0, 1000);
 				ui::slider_int(sk("Box Thickness"), &globals.visuals.box_thickness, 0, 10);
 				ui::slider_int(sk("Skeleton Thickness"), &globals.visuals.skeleton_thickness, 0, 10);
 				ui::slider_int(sk("Snapline Thickness"), &globals.visuals.snapline_thickness, 0, 10);
 				ui::slider_int(sk("Text Size"), &globals.visuals.text_size, 0, 20);
 				ui::binder(sk("Hotkey"), &globals.visuals.master_switch);
 				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 2.0f));
 				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
 				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);

 				ImGui::Combo(sk("Box Style"), &globals.visuals.boxstyle, globals.visuals.boxstyles);
 				ImGui::Combo(sk("Skeleton Style"), &globals.visuals.skeleton_type, globals.visuals.skeleton_types);
 				ImGui::Combo(sk("Health Position"), &globals.visuals.healthposition, globals.visuals.healthpositions);
 				ImGui::Combo(sk("Armor Position"), &globals.visuals.armorposition, globals.visuals.armorpositions);

 				ImGui::PopStyleVar(2);
 			} ui::end_child();

 			
 		}
 		ImGui::EndGroup();
 		});

 		
 		ui::add_page(1, []() {
 			ImGui::BeginGroup();
 			{
 				ui::begin_child(sk("Options")); {
 					ImGui::Checkbox(sk("Activate"), &globals.visuals.veh_activate);
 					ImGui::Checkbox(sk("Box"), &globals.visuals.veh_box, 0, globals.visuals.veh_boxcolor);
 					ImGui::Checkbox(sk("Names"), &globals.visuals.veh_names, 0, globals.visuals.veh_namecolor);
 					ImGui::Checkbox(sk("Distance"), &globals.visuals.veh_distance, 0, globals.visuals.distance_color);
 					ImGui::Checkbox(sk("Snapline"), &globals.visuals.veh_snapline);
 					
 					ImGui::Checkbox(sk("Label Background"), &globals.visuals.veh_label_bg);
 					ImGui::Checkbox(sk("Velocity Line"), &globals.visuals.veh_velocity_line);
 				} ui::end_child();

 				ImGui::SameLine();

 				ui::begin_child(sk("Configuration")); {
 					ui::slider_int(sk("View Distance"), &globals.visuals.veh_view_distance, 0, 2000);
 					ui::slider_int(sk("Snapline Thickness"), &globals.visuals.veh_snapline_thickness, 1, 6);
 					
 					ImGui::Combo(sk("Box Style"), &globals.visuals.veh_boxstyle, globals.visuals.boxstyles);
 					ui::slider_int(sk("Box Thickness"), &globals.visuals.veh_box_thickness, 0, 10);
 					ImGui::Combo(sk("Label Position"), &globals.visuals.veh_label_pos, "Top\0Center\0Bottom\0");
 					ui::slider_int(sk("Max Display"), &globals.visuals.veh_max_display, 0, 200);
 					ui::slider_int(sk("Text Size"), &globals.visuals.veh_text_size, 10, 24);
 					ImGui::Checkbox(sk("Fade With Distance"), &globals.visuals.veh_fade_with_distance);
 				} ui::end_child();
 			}
 			ImGui::EndGroup();
 		});

 	ui::add_page(2, []() {
 		ImGui::BeginGroup();
 		{
 			ui::begin_child(sk("Gameplay")); {

 				ImGui::Checkbox(sk("God-Mode"), &globals.self.godmode);

 				ImGui::Checkbox(sk("Semi-God"), &globals.self.semigodmode);
 				ImGui::Checkbox(sk("Invisible"), &globals.self.Invisible);
 				
 				
 				
 				
 				ImGui::Checkbox(sk("No Collision"), &globals.self.nocol);

 				ImGui::Checkbox(sk("Super Jump"), &globals.self.super_jump);
 				ImGui::Checkbox(sk("Beast Jump"), &globals.self.beast_jump);
 				ImGui::Checkbox(sk("Custom Altitude"), &globals.self.custom_altitude);
 				ImGui::Checkbox(sk("Custom Fov"), &globals.self.custom_fov);
 			} ui::end_child();

 			ui::begin_child(sk("Movement")); {
 				ImGui::Checkbox(sk("Freecam"), &globals.self.freecam);
 				ui::binder(sk("Freecam Key"), &globals.self.freecam_key);
 				
 				ImGui::Checkbox(sk("Teleport On Disable"), &globals.self.freecam_teleport_on_disable);
 				ImGui::Checkbox(sk("Noclip"), &globals.self.noclip);
 				ui::binder(sk("Noclip Key"), &globals.self.hotkey_noclip);
 				

 			} ui::end_child();
 		}
 		ImGui::EndGroup();

 		ImGui::SameLine();

 		ImGui::BeginGroup(); {

 			ui::begin_child(sk("Camera")); {

 				

 				ui::slider_float(sk("Altitude Value"), &globals.self.custom_altitude_value, 0.f, 100.f);
 				ui::slider_float(sk("Fov Value"), &globals.self.custom_fov_value, 0.f, 100.f);
 				ui::slider_int(sk("Freecam Speed"), &globals.self.freecam_speed, 1, 10);
 				ui::slider_int(sk("Noclip Speed"), &globals.self.noclip_speed, 0, 20);

 			} ui::end_child();

 			ui::begin_child(sk("Functions")); {

 				uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);

 				uint64_t localplayer = *(uint64_t*)(world + 0x8);

 				if (ui::modern_button(sk("Heal"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight())))
 				{
 					if (IsPlayerFullyLoaded() && localplayer) {
 						TryWrite<float>(localplayer + 0x280, 100.f);
 					}
 				}
 				if (ui::modern_button(sk("Armor"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight())))
 				{
 					if (IsPlayerFullyLoaded() && localplayer) {
 						TryWrite<float>(localplayer + Address::Get()->m_pArmor, 100.f);
 					}
 				}
 				if (ui::modern_button(sk("Suicide"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight())))
 				{
 					if (IsPlayerFullyLoaded() && localplayer) {
 						TryWrite<float>(localplayer + 0x280, 0.f);
 					}
 				}
 				
 				
 				if (ui::modern_button(sk("ESX Revive"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					ExecuteReviveLua("TriggerEvent('esx_ambulancejob:revive') ");
 				}
 				
 				
 				
 				if (ui::modern_button(sk("Native Revive"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					ReviveLocalPlayer();
 				}
 				

 			} ui::end_child();


 		}
 		ImGui::EndGroup();
 		});

 	ui::add_page(2, []() {
 		ui::begin_child(sk("Options")); {
 			ImGui::Checkbox(sk("Activate"), &globals.weapon.weapon_enabled);

 			ImGui::Checkbox(sk("No Recoil"), &globals.weapon.no_recoil);

 			ImGui::Checkbox(sk("No Spread"), &globals.weapon.no_spread);

 			ImGui::Checkbox(sk("No Reload"), &globals.weapon.no_reload);

 			ImGui::Checkbox(sk("Freeze Ammo"), &globals.weapon.freeze_ammo);

 			ImGui::Checkbox(sk("Range Multiplier"), &globals.weapon.range_multiplier);

 			ImGui::Checkbox(sk("TP To Bullet"), &globals.weapon.tp_to_bullet);

 			ImGui::Checkbox(sk("Bullet Traces"), &globals.weapon.bullet_traces);

 			ImGui::Checkbox(sk("Damage Multiplier"), &globals.weapon.damage_boost);

 			ImGui::Checkbox(sk("Infinite Ammo"), &globals.weapon.infinite_ammo);

 			ImGui::Checkbox(sk("Explosive Ammo"), &globals.weapon.explosiveammo);

 			ImGui::Checkbox(sk("Fire Ammo"), &globals.weapon.fire_ammo);
 		} ui::end_child();

 		ImGui::SameLine();

 		ImGui::BeginGroup();
 		{
 			
 			ui::begin_child(sk("Settings")); {
 				ui::slider_int(sk("Range"), &globals.weapon.weapon_range, 0, 1000);
 				ui::slider_int(sk("Damage"), &globals.weapon.weapon_damage, 0, 1000);
 				ImGui::ColorEdit4(sk("Bullet Traces Col"), (float*)&globals.weapon.bullet_traces_col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoBorder);
 			} ui::end_child();

 			
 			ui::begin_child(sk("Give weapon")); {
 				ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 10, 210));
 				int __visibleRows = 8;
 				ImVec2 __listSize = ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * __visibleRows);
 				if (ImGui::BeginListBox(sk("##weapon_list"), __listSize)) {
 					ImGuiListClipper clipper;
 					clipper.Begin(IM_ARRAYSIZE(weapon_list));
 					while (clipper.Step()) {
 						for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
 							bool sel = (globals.weapon.weapon_spawn_index == i);
 							if (ImGui::Selectable(weapon_list[i], sel)) {
 								globals.weapon.weapon_spawn_index = i;
 							}
 							if (sel) ImGui::SetItemDefaultFocus();
 						}
 					}
 					ImGui::EndListBox();
 				}
 				ImGui::PopStyleColor();

 				if (ui::modern_button("Give Weapon", ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					if (globals.weapon.weapon_spawn_index != -1) {
 						globals.weapon.weapon_spawn = true;
 					}
 				}
 			} ui::end_child();
 		}
 		ImGui::EndGroup();
 		});


 	
 	ui::add_page(2, []() {
 		static char searchBuffer[128] = "";
 		ImGui::BeginGroup();
 		{
 			
 			float totalW = ImGui::GetContentRegionAvail().x;
 			float spacing = ImGui::GetStyle().ItemSpacing.x;
 			float leftW = totalW * 0.60f;
 			float rightW = totalW - leftW - spacing;
 			
 			ui::begin_child(sk("General")); {
 				ImGui::Checkbox(sk("Rocket Boost (E)"), &globals.vehicle.rocket_boost);
 				ImGui::Checkbox(sk("Shift Boost"), &globals.vehicle.shift_boost);
 				ImGui::Checkbox(sk("Godmode"), &globals.vehicle.godmode);
 				ImGui::Checkbox(sk("Modify Gravity"), &globals.vehicle.modify_gravity);
 				if (ui::modern_button(sk("Spawn Vehicle"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					if (IsPlayerFullyLoaded()) {
 						unsigned int vh = (unsigned)globals.vehicle.spawn_vehicle_hash;
 						if (vh == 1475773103u || (MemoryAddress::is_model_in_cd_image && !MemoryAddress::is_model_in_cd_image(vh))) {
 							vh = (unsigned)VEHICLE_ADDER;
 							globals.vehicle.spawn_vehicle_hash = (int)vh;
 						}
 						if (MemoryAddress::create_vehicle) {
 							globals.vehicle.spawn_vehicle = true;
 						}
 						else {
 							SpawnVehicleViaLuaHash(vh);
 						}
 					}
 				}

 				ImGui::Separator();
 				ImGui::TextUnformatted("Vehicle Spawner");
 				ImGui::SetNextItemWidth(-1);
 				ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 10, 10, 210));
 				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(14, 14, 14, 230));
 				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(18, 18, 18, 255));
 				ImGui::InputTextWithHint(sk("##veh_search"), sk("Search vehicle..."), searchBuffer, IM_ARRAYSIZE(searchBuffer));
 				ImGui::PopStyleColor(3);



 				

 				
 				ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 10, 210));
 				ImGui::BeginChild(sk("##veh_list_child"), ImVec2(-1, 160), false, ImGuiWindowFlags_NoScrollbar);
 				if (ImGui::BeginListBox(sk("##veh_list"), ImVec2(-1, -1))) {
 					for (size_t i = 0; i < ServerVehicleList.size(); ++i) {
 						const auto& vehicle = ServerVehicleList[i];
 						if (strlen(searchBuffer) > 0 && std::string(vehicle.name).find(searchBuffer) == std::string::npos) continue;
 						bool isSelected = (globals.vehicle.spawn_vehicle_hash == vehicle.hash);
 						std::string display = vehicle.name.empty() ? std::string("Unknown") : vehicle.name;
 						ImGui::PushID((int)vehicle.hash);
 						if (ImGui::Selectable(display.c_str(), isSelected)) {
 							globals.vehicle.spawn_vehicle_hash = vehicle.hash;
 						}

 						ImGui::PopID();
 					}

 					ImGui::EndListBox();
 					
 				}
 				ImGui::EndChild();
 				ImGui::PopStyleColor();

 				

 			} ui::end_child();

 			
 			ImGui::SameLine();
 			ImGui::BeginGroup();
 			{
 				ui::begin_child(sk("Options")); {
 					
 					ImGui::SetNextItemWidth(ImGui::CalcItemWidth());
 					ui::slider_int(sk("Shift Boost Multiplier"), &globals.vehicle.shift_boost_value, 0, 100);
 					ImGui::SetNextItemWidth(ImGui::CalcItemWidth());
 					ui::slider_int(sk("Gravity Multiplier"), &globals.vehicle.gravity_value, 0, 100);
 					ImGui::Dummy(ImVec2(0, 6));					
 					ImGui::ColorEdit4(sk("Primary Color"), (float*)&(globals.vehicle.primary_vehicle_color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoBorder);
 					ImGui::ColorEdit4(sk("Secondary Color"), (float*)&(globals.vehicle.secondary_vehicle_color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoBorder);
 					ImGui::Dummy(ImVec2(0, 6));					
 					if (ui::modern_button(sk("Apply"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 						globals.vehicle.apply_color_change = true;
 					}
 					if (ui::modern_button(sk("Repair"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight())))
 					{
 						globals.vehicle.repair = true;
 					}
 					
 					
 					ImGui::Separator();
 					ImGui::InputText(sk("Other##veh_plate"), globals.vehicle.plate_text, IM_ARRAYSIZE(globals.vehicle.plate_text));
 					if (ui::modern_button(sk("Set Plate"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 						globals.vehicle.set_plate = true;
 					}
 					
 				} ui::end_child();
 			}
 			ImGui::EndGroup();

 		}
 		ImGui::EndGroup();

 		
 		if (!updatedVehList || ServerVehicleList.empty()) {
 			updatedVehList = true;
 			PopulateServerVehicleList();
 		}
 		});

 	
 	ui::add_page(2, []() {
 		static char searchBuffer[128] = "";
 		
 		ImGui::BeginGroup();
 		{
 			ui::begin_child(sk("Teleport Locations")); {
 				ImGui::TextUnformatted("Teleport Locations");
 				ImGui::SetNextItemWidth(-1);
 				ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 10, 10, 210));
 				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(14, 14, 14, 230));
 				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(18, 18, 18, 255));
 				ImGui::InputTextWithHint(sk("##teleport_search"), sk("Search location..."), searchBuffer, IM_ARRAYSIZE(searchBuffer));
 				ImGui::PopStyleColor(3);

 				
 				ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 10, 210));
 				ImGui::BeginChild(sk("##teleport_list_child"), ImVec2(-1, 200), false, ImGuiWindowFlags_NoScrollbar);
 				if (ImGui::BeginListBox(sk("##teleport_list"), ImVec2(-1, -1))) {
 					for (int i = 0; i < g_TeleportLocationsCount; i++) {
 						if (strlen(searchBuffer) > 0 && std::string(g_TeleportLocations[i].name).find(searchBuffer) == std::string::npos) continue;
 						bool isSelected = (globals.self.selected_teleport_index == i);
 						ImGui::PushID(i);
 						if (ImGui::Selectable(g_TeleportLocations[i].name, isSelected)) {
 							globals.self.selected_teleport_index = i;
 						}
 						ImGui::PopID();
 					}
 					ImGui::EndListBox();
 				}
 				ImGui::EndChild();
 				ImGui::PopStyleColor();
 			} ui::end_child();
 		}
 		ImGui::EndGroup();

 		
 		ImGui::SameLine();
 		ImGui::BeginGroup();
 		{
 			ui::begin_child(sk("Options")); {
 				
 				if (globals.self.selected_teleport_index >= 0 && globals.self.selected_teleport_index < g_TeleportLocationsCount) {
 					const TeleportLocation& location = g_TeleportLocations[globals.self.selected_teleport_index];
 					ImGui::Text(sk("Selected: %s"), location.name);
 					ImGui::Text(sk("Coordinates: %.1f, %.1f, %.1f"), location.x, location.y, location.z);
 					ImGui::Separator();
 				}
 				
 				
 				if (ui::modern_button(sk("Teleport"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					if (globals.self.selected_teleport_index >= 0 && globals.self.selected_teleport_index < g_TeleportLocationsCount) {
 						uint64_t pedfactory = *(uint64_t*)(Address::Get()->m_sPedFactory);
 						if (pedfactory) {
 							uint64_t localplayer = *(uint64_t*)(pedfactory + 0x8);
 							if (localplayer) {
 								const TeleportLocation& location = g_TeleportLocations[globals.self.selected_teleport_index];
 								if (Safe_SetEntityCoordsNoOffset(localplayer, location.x, location.y, location.z, true, true, true, true)) {
 									if (Address::Get()->m_pVelocity) {
 										SafeWriteVec3(localplayer + Address::Get()->m_pVelocity, Vector3(0.f, 0.f, 0.f));
 									}
 								}
 							}
 						}
 					}
 				}
 				
 				
 				if (ui::modern_button(sk("Random Location"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					globals.self.selected_teleport_index = rand() % g_TeleportLocationsCount;
 				}
 				
 				
 				if (ui::modern_button(sk("Teleport to Waypoint"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					Vector3 wp = GetWaypointCoords();
 					if (!(wp.x == 0.f && wp.y == 0.f)) {
 						uint64_t pedfactory = *(uint64_t*)(Address::Get()->m_sPedFactory);
 						uint64_t localplayer = pedfactory ? *(uint64_t*)(pedfactory + 0x8) : 0;
 						if (localplayer) {
 							float tx = wp.x, ty = wp.y, tz = wp.z + 1.0f;
 							if (!Safe_SetEntityCoordsNoOffset(localplayer, tx, ty, tz, true, false, false, true)) {
 								SafeWriteVec3(localplayer + 0x90, Vector3(tx, ty, tz));
 								SafeWriteVec3(localplayer + 0x320, Vector3(tx, ty, tz));
 								uint64_t navptr = 0; if (TryRead<uint64_t>(localplayer + 0x30, &navptr) && navptr) SafeWriteVec3(navptr + 0x50, Vector3(tx, ty, tz));
 							}
 							if (Address::Get()->m_pVelocity) {
 								SafeWriteVec3(localplayer + Address::Get()->m_pVelocity, Vector3(0.f, 0.f, 0.f));
 							}
 						}
 					}
 				}
 			} ui::end_child();
 		}
 		ImGui::EndGroup();
 	});

 	ui::add_page(3, []() {

 		ui::begin_child(sk("Player List")); {

 			
 			static double s_lastPlayersRefresh = 0.0;
 			static std::vector<std::pair<void*, int>> s_cachedPlayers; 
 			static std::vector<std::string> s_cachedNames;
 			bool needRefresh = (ImGui::GetTime() - s_lastPlayersRefresh) > 0.2; 
			if (needRefresh) {
				s_cachedPlayers.clear();
				s_cachedNames.clear();

				uint64_t world_lp = 0;
				TryRead<uint64_t>(Address::Get()->m_sPedFactory, &world_lp);
				uint64_t lp = 0;
				if (world_lp) TryRead<uint64_t>(world_lp + 0x8, &lp);

				float lpx = 0.f, lpy = 0.f, lpz = 0.f;
				bool have_local = GetEntityCoordsFast(lp, &lpx, &lpy, &lpz);

				if (Safe_ProcessPedPool()) {
					if (!BuildPlayerCacheSEH(s_cachedPlayers, s_cachedNames, have_local, lpx, lpy, lpz)) {
						s_cachedPlayers.clear();
						s_cachedNames.clear();
					}
				}
				s_lastPlayersRefresh = ImGui::GetTime();
			}

 			
 			ImGui::SetNextItemWidth(-1);
 			ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 10, 10, 210));
 			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(14, 14, 14, 230));
 			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(18, 18, 18, 255));
 			ImGui::InputTextWithHint(sk("##player_search"), sk("Search player..."), globals.player_list_search, IM_ARRAYSIZE(globals.player_list_search));
 			ImGui::PopStyleColor(3);

 			
 			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 10, 210));
 			ImGui::BeginChild(sk("##player_list_child"), ImVec2(-1, 200), false, ImGuiWindowFlags_NoScrollbar);
 			if (ImGui::BeginListBox(sk("##playerblablabl"), ImVec2(-1, -1))) {
 				size_t index = 0;
 				for (size_t i = 0; i < s_cachedPlayers.size(); ++i) {
 					void* cPed = s_cachedPlayers[i].first;
 					int dist_m = s_cachedPlayers[i].second;
 					std::string display_name = s_cachedNames[i];
 					if (display_name == "Player") display_name += (" " + std::to_string(i));
 					if (strlen(globals.player_list_search) > 0 && display_name.find(globals.player_list_search) == std::string::npos) continue;
 					char label_buf[256];
 					if (dist_m > 0) _snprintf_s(label_buf, sizeof(label_buf), _TRUNCATE, "%s (%dm)", display_name.c_str(), dist_m);
 					else _snprintf_s(label_buf, sizeof(label_buf), _TRUNCATE, "%s", display_name.c_str());
 					bool is_selected = (globals.index.selectedPlayerIndex == index);
 					ImGui::PushID((void*)cPed);
 					if (ImGui::Selectable(label_buf, is_selected)) {
 						globals.index.selectedPlayerIndex = (int)index;
 						globals.index.selected_player = (uint64_t)cPed;
 					}
 					ImGui::PopID();
 					index++;
 				}
 				ImGui::EndListBox();
 			}
 			ImGui::EndChild();
 			ImGui::PopStyleColor();

 		} ui::end_child();

 		ImGui::SameLine();


 		
 		static double tp_stick_until_time = 0.0; 
 		static float tp_stick_x = 0.f, tp_stick_y = 0.f, tp_stick_z = 0.f;
 		static bool tp_collision_temporarily_disabled = false;

 		ui::begin_child("Player Options");
 		{
 			if (ui::modern_button(sk("Kill Player"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t target = globals.index.selected_player;
 				if (IsPlayerFullyLoaded() && target) {
 					
 					TryWrite<float>(target + Address::Get()->m_pArmor, 0.f);
 					TryWrite<float>(target + 0x280, 0.f);
 				}
 			}

 			if (ui::modern_button(sk("Launch Player"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t target = globals.index.selected_player;
 				if (IsPlayerFullyLoaded() && target) {
 					
 					float tx = 0.f, ty = 0.f, tz = 0.f;
 					if (GetEntityCoordsSafe(target, &tx, &ty, &tz)) {
 						
 						bool success = false;

 						
 						if (Safe_SetEntityCoordsNoOffset(target, tx, ty, tz + 100.0f, false, false, false, true)) {
 							success = true;
 						}

 						
 						if (!success && Safe_SetEntityCoordsNoOffset(target, tx, ty, tz + 100.0f, false, false, false, false)) {
 							success = true;
 						}

 						
 						if (!success) {
 							if (TryWrite<float>(target + 0x90, tx) &&
 								TryWrite<float>(target + 0x94, ty) &&
 								TryWrite<float>(target + 0x98, tz + 100.0f)) {
 								
 								uint64_t nav = 0;
 								if (TryRead<uint64_t>(target + 0x30, &nav) && nav) {
 									TryWrite<float>(nav + 0x50, tx);
 									TryWrite<float>(nav + 0x54, ty);
 									TryWrite<float>(nav + 0x58, tz + 100.0f);
 								}
 							}
 						}

 						
 						if (Address::Get()->m_pVelocity) {
 							TryWrite<float>(target + Address::Get()->m_pVelocity + 0x8, 50.0f); 
 						}
 					}
 				}
 			}

 			if (ui::modern_button(sk("Crash Player (FiveM)"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t target = globals.index.selected_player;
 				if (IsPlayerFullyLoaded() && target) {
 					CrashPlayerViaLua(target);
 				}
 			}

 			
 			{
 				uint64_t target = globals.index.selected_player;
 				uint64_t netid = 0;
 				if (target) {
 					uint64_t playerinfo = 0;
 					TryRead<uint64_t>(target + Address::Get()->m_pPlayerInfo, &playerinfo);
 					if (playerinfo) TryRead<uint64_t>(playerinfo + Address::Get()->m_pNetid, &netid);
 				}
 				bool is_friend = (netid && (g_friend_netids.find(netid) != g_friend_netids.end()));
 				if (netid) {
 					if (!is_friend) {
 						if (ui::modern_button(sk("Add Friend"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 							g_friend_netids.insert(netid);
 						}
 					} else {
 						if (ui::modern_button(sk("Unfriend"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 							g_friend_netids.erase(netid);
 						}
 					}
 				} else {
 					ImGui::BeginDisabled();
 					ui::modern_button(sk("Add Friend"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()));
 					ImGui::EndDisabled();
 				}
 			}

 			/*if (ui::modern_button(sk("Explode"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t target = globals.index.selected_player;
 				if (IsPlayerFullyLoaded() && target) {
 					float tx=0.f, ty=0.f, tz=0.f;
 					if (GetEntityCoordsSafe(target, &tx, &ty, &tz) && MemoryAddress::add_explosion) {
 						// Explosion type 2 (grenade-like), damageScale 5.0, audible true, invisible false, small cameraShake
 						MemoryAddress::add_explosion(tx, ty, tz, 2, 5.0f, true, false, 0.2f);
 					}
 				}
 			}

 			if (ui::modern_button(sk("Explode Vehicle"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t target = globals.index.selected_player;
 				if (IsPlayerFullyLoaded() && target) {
 					uint64_t veh = 0;
 					if (TryRead<uint64_t>(target + Address::Get()->m_pVehMgr, &veh) && veh && MemoryAddress::explose_vehicule) {
 						MemoryAddress::explose_vehicule(veh, true, false);
 					}
 				}
 			}*/

 			if (ui::modern_button(sk("TP To Player"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t target = globals.index.selected_player;
 				if (target) {
 					uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
 					uint64_t localplayer = world ? *(uint64_t*)(world + 0x8) : 0;
 					if (localplayer) {
 						
 						float tx = 0.f, ty = 0.f, tz = 0.f;
 						bool have_target = GetEntityCoordsSafe(target, &tx, &ty, &tz);
 						if (!have_target) {
 							
 							goto __skip_tp_player;
 						}

 						if (MemoryAddress::SetEntityCoordsNoOffset) {
 							if (!Safe_SetEntityCoordsNoOffset(localplayer, tx, ty, tz + 1.0f, false, false, false, true)) {
 								TryWrite<float>(localplayer + 0x90, tx);
 								TryWrite<float>(localplayer + 0x94, ty);
 								TryWrite<float>(localplayer + 0x98, tz + 1.0f);
 								TryWrite<float>(localplayer + 0x320, tx);
 								TryWrite<float>(localplayer + 0x324, ty);
 								TryWrite<float>(localplayer + 0x328, tz + 1.0f);
 							}
 						} else {
 							TryWrite<float>(localplayer + 0x90, tx);
 							TryWrite<float>(localplayer + 0x94, ty);
 							TryWrite<float>(localplayer + 0x98, tz + 1.0f);
 							TryWrite<float>(localplayer + 0x320, tx);
 							TryWrite<float>(localplayer + 0x324, ty);
 							TryWrite<float>(localplayer + 0x328, tz + 1.0f);
 						}
 					}
 				}
 				__skip_tp_player: ;
 			}

 			
 			static char s_propModel[64] = "prop_tree_cedar_02"; 
 			ImGui::InputText(sk("Prop model"), s_propModel, IM_ARRAYSIZE(s_propModel));
 			if (ui::modern_button(sk("Spawn prop near player"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t target = globals.index.selected_player;
 				if (target) {
 					float tx=0.f, ty=0.f, tz=0.f;
 					if (GetEntityCoordsSafe(target, &tx, &ty, &tz)) {
 						
 						SpawnPropViaLuaAt(s_propModel, tx, ty, tz);
 					}
 				}
 			}

 	if (ui::modern_button(sk("Spectate Player"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
     uint64_t target = globals.index.selected_player;
     if (!target) {
         
         uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
         uint64_t localplayer = world ? *(uint64_t*)(world + 0x8) : 0;
         if (Address::Get()->s_mPedPool) {
             for (auto cPed : **Address::Get()->s_mPedPool) {
                 if (!cPed || cPed == localplayer) continue;
                 auto ped_type = *(uint32_t*)((uint64_t)cPed + Address::Get()->m_pEntityType);
                 if (!ped_type) continue;
                 ped_type = ped_type << 11 >> 25;
                 if (ped_type == 2) { target = cPed; break; }
             }
         }
     }
     if (target) {
         if (MemoryAddress::spectate && MemoryAddress::pointer_to_handle) {
             int handle = MemoryAddress::pointer_to_handle((intptr_t)target);
             MemoryAddress::spectate(true, handle);
         }
         globals.self.spectate = true;
         globals.self.spectate_target = target;
         globals.self.freecam = false;
     }
 }

 if (ui::modern_button(sk("Unspectate"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
     uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
     uint64_t localplayer = world ? *(uint64_t*)(world + 0x8) : 0;
     if (localplayer && MemoryAddress::spectate && MemoryAddress::pointer_to_handle) {
         int me = MemoryAddress::pointer_to_handle((intptr_t)localplayer);
         MemoryAddress::spectate(false, me);
     }
     globals.self.spectate = false;
     globals.self.spectate_target = 0;
 }


 		}
 		ui::end_child();

 		
 		if (tp_stick_until_time > ImGui::GetTime()) {
 			uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
 			uint64_t localplayer = world ? *(uint64_t*)(world + 0x8) : 0;
 			if (localplayer) {
 				if (Safe_SetEntityCoordsNoOffset(localplayer, tp_stick_x, tp_stick_y, tp_stick_z, true, false, false, true)) {
 					uint64_t lnav = *(uint64_t*)(localplayer + 0x30);
 					if (lnav) {
 						Safe_SetEntityCoordsNoOffset(lnav, tp_stick_x, tp_stick_y, tp_stick_z, true, false, false, true);
 					}
 					if (Address::Get()->m_pVelocity) {
 						Safe_SetEntityCoordsNoOffset(localplayer, tp_stick_x, tp_stick_y, tp_stick_z, true, false, false, true);
 					}
 					
 					if (tp_collision_temporarily_disabled) {
 						uint64_t model_info = *(uint64_t*)(localplayer + 0x20);
 						if (model_info) Safe_SetEntityCoordsNoOffset(model_info, 0.f, 0.f, 0.f, true, false, false, true);
 					}
 				}
 			}
 		} else if (tp_collision_temporarily_disabled) {
 			
 			uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
 			uint64_t localplayer = world ? *(uint64_t*)(world + 0x8) : 0;
 			if (localplayer) {
 				uint64_t model_info = *(uint64_t*)(localplayer + 0x20);
 				if (model_info) Safe_SetEntityCoordsNoOffset(model_info, 0.f, 0.f, 0.f, true, false, false, true);
 			}
 			tp_collision_temporarily_disabled = false;
 		}
 		});



 	ui::add_page(4, []() {
		if (!EnsureLuaTabConfirmed(sk("Resources"))) {
			return;
		}
 		
 		ui::begin_child(sk("Resources"), ImVec2(ImGui::GetContentRegionAvail().x, 0)); {
 			ImVec2 avail = ImGui::GetContentRegionAvail();
 			float totalW = avail.x;
 			float spacing = ImGui::GetStyle().ItemSpacing.x;
 			float leftW = totalW * 0.60f;
 			float rightW = totalW - leftW - spacing;

 			
 			ImGui::BeginGroup();
 			{
 				ImGui::SetNextItemWidth(leftW);
 				ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 10, 10, 210));
 				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(14, 14, 14, 230));
 				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(18, 18, 18, 255));
 				ImGui::InputTextWithHint("Filter##res_search", "Filter resources...", lua_res_filter, IM_ARRAYSIZE(lua_res_filter));
 				ImGui::PopStyleColor(3);

 				auto resources2 = GetResourcesSafe();
 				float rightW = totalW - leftW - spacing;
 				float baseH = ImGui::GetTextLineHeight() * 15.0f;
 				ImVec2 listSize = ImVec2(leftW, baseH);
 				ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 10, 210));
 				if (ImGui::BeginListBox(sk("##res_list"), listSize)) {
 					for (int i = 0; i < (int)resources2.size(); ++i) {
 						if (!resources2[i].GetRef()) continue;
 						std::string nameStr = resources2[i]->get_impl()->GetName();
 						if (lua_res_filter[0] != '\0' && nameStr.find(lua_res_filter) == std::string::npos) continue;
 						bool isSelected = (lua_selected_resource == i);
 						ImGui::PushID(i);
 						if (ImGui::Selectable(nameStr.c_str(), isSelected)) {
 							lua_selected_resource = i;
 						}
 						ImGui::PopID();
 					}
 					ImGui::EndListBox();
 				}
 				ImGui::PopStyleColor();
 				
 				if (lua_selected_resource >= (int)resources2.size()) lua_selected_resource = (int)resources2.size() - 1;
 				if (lua_selected_resource < 0 && !resources2.empty()) lua_selected_resource = 0;
 			}
 			ImGui::EndGroup();

 			ImGui::SameLine();

 			
 			ImGui::BeginGroup();
 			{
 				ImVec2 __rAvail = ImGui::GetContentRegionAvail();
 				float rightW = totalW - leftW - spacing;
 				float baseH = ImGui::GetTextLineHeight() * 9.0f;
 				float __detailsW = rightW * 0.60f; 
 				float __detailsH = baseH * 3.0f; 
 				ImGui::BeginChild(sk("##res_details"), ImVec2(__detailsW, __detailsH), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
 				auto resources2 = GetResourcesCached();
 				bool hasSel = (lua_selected_resource >= 0 && lua_selected_resource < (int)resources2.size());
 				if (hasSel) {
 					auto r = resources2[lua_selected_resource];
 					const bool refOk = r.GetRef();
 					std::string nameStr = refOk ? r->get_impl()->GetName() : std::string();
 					bool isStoppedByUser = (!nameStr.empty() && s_userStopped[nameStr]);
 					bool isRunning = !isStoppedByUser;
 					const char* statusTxt = isRunning ? "Running..." : "Stopped";
 					ImGui::Text("Status: %s", statusTxt);

 					ImGui::Dummy(ImVec2(0, 6));
 					
 					ImGui::BeginDisabled(!refOk || !isStoppedByUser);
 					if (ui::modern_button(sk("Start"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 						if (r.GetRef()) { r->Start(); if (!nameStr.empty()) s_userStopped[nameStr] = false; }
 					}
 					ImGui::EndDisabled();

 					ImGui::BeginDisabled(!refOk || isStoppedByUser);
 					if (ui::modern_button(sk("Stop"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 						if (r.GetRef()) { r->Stop(); if (!nameStr.empty()) s_userStopped[nameStr] = true; }
 					}
 					ImGui::EndDisabled();

 					if (ui::modern_button(sk("Restart"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 						if (r.GetRef()) { r->Stop(); Sleep(50); r->Start(); if (!nameStr.empty()) s_userStopped[nameStr] = false; }
 					}


 				} else {
 					ImGui::TextDisabled("No resource selected");
 				}
 				ImGui::Separator();
 				/*if (ui::modern_button(sk("Start All"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					for (auto& r : resources2) { if (r.GetRef()) r->Start(); }
 				}
 				if (ui::modern_button(sk("Stop All"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 					for (auto& r : resources2) { if (r.GetRef()) r->Stop(); }
 				}*/
 				
 				
 			
 			

 				ImGui::EndChild();
 			}
 			ImGui::EndGroup();
 		} ui::end_child();
 		});

 	ui::add_page(4, []() {
		if (!EnsureLuaTabConfirmed(sk("Lua"))) {
			return;
		}
 		
 		ui::begin_child(sk("Lua"), ImVec2(ImGui::GetContentRegionAvail().x, 0)); {
 			#if 0
 			
 			ImGui::BeginGroup();
 			{
 				auto resourcesExec = GetResourcesCached();
 				ImGui::SetNextItemWidth(220.0f);
 				const char* resLabel = "<none>";
 				if (!resourcesExec.empty() && lua_selected_resource >= 0 && lua_selected_resource < (int)resourcesExec.size())
 					resLabel = resourcesExec[lua_selected_resource]->get_impl()->GetName().c_str();
 				if (ImGui::BeginCombo(sk("Resource"), resLabel)) {
 					for (int i = 0; i < (int)resourcesExec.size(); ++i) {
 						std::string rname = resourcesExec[i]->get_impl()->GetName();
 						bool sel = (lua_selected_resource == i);
 						if (ImGui::Selectable(rname.c_str(), sel)) lua_selected_resource = i;
 						if (sel) ImGui::SetItemDefaultFocus();
 					}
 					ImGui::EndCombo();
 				}
 				ImGui::SameLine();
 				if (ui::modern_button(sk("Open"), ImVec2(90, 0))) {
 					std::wstring pathW;
 					if (OpenLuaFileDialog(pathW)) {
 						std::string data = ReadFileToString(pathW.c_str());
 						if (!data.empty()) {
 							strncpy_s(lua_script_buf, data.c_str(), _TRUNCATE);
 							wcsncpy_s(lua_last_path, pathW.c_str(), _TRUNCATE);
 							AppendLuaLog("Opened script.");
 						}
 					}
 				}
 				ImGui::SameLine();
 				if (ui::modern_button(sk("Save As"), ImVec2(90, 0))) {
 					std::wstring pathW;
 					if (SaveLuaFileDialog(pathW)) {
 						size_t len = strlen(lua_script_buf);
 						if (WriteStringToFile(pathW.c_str(), lua_script_buf, len)) {
 							wcsncpy_s(lua_last_path, pathW.c_str(), _TRUNCATE);
 							AppendLuaLog("Saved script.");
 						}
 					}
 				}
 				ImGui::SameLine();
 				if (ui::modern_button(sk("Copy"), ImVec2(80, 0))) {
 					ImGui::SetClipboardText(lua_script_buf);
 					AppendLuaLog("Copied to clipboard.");
 				}
 				ImGui::SameLine();
 				if (ui::modern_button(sk("Paste"), ImVec2(80, 0))) {
 					const char* clip = ImGui::GetClipboardText();
 					if (clip) {
 						strncpy_s(lua_script_buf, clip, _TRUNCATE);
 						AppendLuaLog("Pasted from clipboard.");
 					}
 				}
 				ImGui::SameLine();
 				ImGui::Checkbox(sk("Auto-scroll console"), &lua_console_autoscroll);
 			}
 			ImGui::EndGroup();
 			ImGui::Separator();
 			#endif

 			
 			ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
 			float editorHeight = ImGui::GetTextLineHeight() * 24.0f;
 			ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 10, 10, 210));
 			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
 			ImGui::BeginChild(sk("##lua_editor"), ImVec2(-1, editorHeight), true, ImGuiWindowFlags_NoScrollbar);
 			{
 				ImGui::PushFont(g_LuaMonoFont ? g_LuaMonoFont : ImGui::GetFont());
 				ImGui::InputTextMultiline(sk("##lua_script"), lua_script_buf, IM_ARRAYSIZE(lua_script_buf), ImVec2(-1, -1), flags);
 				ImGui::PopFont();
 			}
 			ImGui::EndChild();
 			ImGui::PopStyleVar();
 			ImGui::PopStyleColor();

 			ImGui::Dummy(ImVec2(0, 6));
 			if (ui::modern_button(sk("Execute"), ImVec2(90, 0))) {
				std::string script(lua_script_buf);
				if (IsLuaReady()) {
					DirectLuaExec(script);
					AppendLuaLog("[Pink] DirectLua: OK");
				} else {
					AppendLuaLog("[Pink] DirectLua not ready, fallback RPC");
					SafeLuaExecScript(script);
				}
			}
 			ImGui::SameLine();
 			if (ui::modern_button(sk("Clear"), ImVec2(90, 0))) { lua_script_buf[0] = '\0'; }
 			ImGui::SameLine();

 			/*ImGui::SetNextItemWidth(160.0f);
 			int __comboFlags = ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLargest;
 			if (ImGui::BeginCombo(sk("##res_select"), resNames.empty() ? "" : resNames[lua_selected_resource].c_str(), __comboFlags)) {
 				// Rebuild list on open in case resources appeared just now
 				resNames.clear();
 				resMap.clear();
 				resourcesExec2 = GetResourcesCached();
 				if (resourcesExec2.empty()) resourcesExec2 = GetResourcesSafe();
 				for (int i = 0; i < (int)resourcesExec2.size(); ++i) {
 					if (!resourcesExec2[i].GetRef()) continue;
 					std::string nm = resourcesExec2[i]->get_impl()->GetName();
 					if (nm.empty()) continue;
 					resMap.emplace_back(i);
 					resNames.emplace_back(std::move(nm));
 				}
 				if (resNames.empty()) {
 					ImGui::Selectable(sk("<no resources>"), false, ImGuiSelectableFlags_Disabled);
 				} else {
 					for (int i = 0; i < (int)resNames.size(); ++i) {
 						bool sel = (lua_selected_resource == i);
 						if (ImGui::Selectable(resNames[i].c_str(), sel)) lua_selected_resource = i;
 						if (sel) ImGui::SetItemDefaultFocus();
 					}
 				}
 				ImGui::EndCombo();
 			}*/
 			ImGui::SameLine();
 			if (ui::modern_button(sk("Open Lua"), ImVec2(90, 0))) {
 				std::wstring pathW;
 				if (OpenLuaFileDialog(pathW)) {
 					std::string data = ReadFileToString(pathW.c_str());
 					if (!data.empty()) {
 						strncpy_s(lua_script_buf, data.c_str(), _TRUNCATE);
 						wcsncpy_s(lua_last_path, pathW.c_str(), _TRUNCATE);
 						AppendLuaLog("Opened script.");
 					}
 				}
 			}
 			ImGui::SameLine();
 			{
 				auto r = PickSafeResource();
 				const char* rn = (r.GetRef() ? r->get_impl()->GetName().c_str() : "<none>");
 				ImGui::TextDisabled("Target: %s", rn);
 			}

 			#if 0
 			
 			#endif
 		} ui::end_child();
 		});

 	
 	ui::add_page(5, []() {
 		ui::begin_child(sk(" Settings")); {
 			ImGui::Checkbox(sk("Watermark"), &globals.visuals.watermark);
 			ImGui::Checkbox(sk("Block Input"), &globals.menu_settings.block_input);
 			ImGui::Checkbox(sk("Crosshair"), &globals.menu_settings.crosshair, 0, globals.menu_settings.crosshair_color);
 			
 			ui::binder(sk("Menu Hotkey"), &globals.menu.hotkey);
 			
 			
 			
 			
 			ImGui::Dummy(ImVec2(0, 6));
 			ImGui::Separator();
 			
 			static uint64_t s_adhesiveAddr = 0ULL;
 			static char s_adhesiveBytes[64] = "";
 			static char s_adhesiveStatus[128] = "";
 			ImGui::TextUnformatted("Adhesive pattern tester");
 			if (ui::modern_button(sk("Check Adhesive"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				const char* pattern = "49 8B 08 48 3B D1"; 
 				s_adhesiveAddr = 0ULL;
 				s_adhesiveBytes[0] = '\0';
 				s_adhesiveStatus[0] = '\0';
 				const char* modules[] = { "adhesive.dll", "adhesive" };
 				for (int i = 0; i < 2 && s_adhesiveAddr == 0; ++i) {
 					HMODULE hMod = GetModuleHandleA(modules[i]);
 					if (!hMod) continue;
 					s_adhesiveAddr = Scanner::Get()->ReturnScan((uint64_t)hMod, pattern, 0);
 				}
 				if (s_adhesiveAddr) {
 					_snprintf_s(s_adhesiveStatus, _countof(s_adhesiveStatus), _TRUNCATE, "Found at 0x%llX", (unsigned long long)s_adhesiveAddr);
 					if (IsReadableAddress((const void*)s_adhesiveAddr, 6)) {
 						const unsigned char* p = (const unsigned char*)s_adhesiveAddr;
 						_snprintf_s(s_adhesiveBytes, _countof(s_adhesiveBytes), _TRUNCATE,
 							"Bytes: %02X %02X %02X %02X %02X %02X",
 							(unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3], (unsigned)p[4], (unsigned)p[5]);
 					}
 					else {
 						_snprintf_s(s_adhesiveBytes, _countof(s_adhesiveBytes), _TRUNCATE, "Bytes: (unreadable)");
 					}
 				}
 				else {
 					_snprintf_s(s_adhesiveStatus, _countof(s_adhesiveStatus), _TRUNCATE, "Not found / module not loaded");
 				}
 			}
 			if (s_adhesiveStatus[0] != '\0') ImGui::TextUnformatted(s_adhesiveStatus);
 			if (s_adhesiveBytes[0] != '\0') ImGui::TextUnformatted(s_adhesiveBytes);
 			if (ui::modern_button(sk("Unload (safe)"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				globals.menu.unload_requested = true;
 				globals.menu.opened = false;
 			}
 		} ui::end_child();

 		});

 	
 	ui::add_page(3, []() {
 		
 		static double s_lastSnapTime = 0.0;
 		struct CachedVehItem { void* addr; int dist_m; unsigned int hash; char label[96]; };
 		static std::vector<CachedVehItem> s_cachedVeh;
 		static std::vector<void*> s_allVehSnapshot;
 		static size_t s_scanIndex = 0;
 		static float s_lastLpX = 0.f, s_lastLpY = 0.f, s_lastLpZ = 0.f;
 		static int s_maxRows = 15;       
 		static int s_maxDistance = 50;   
 		static int s_batchPerFrame  = 48;  
 		bool poolAvailable = (Address::Get()->s_mVehiclePool && *Address::Get()->s_mVehiclePool && **Address::Get()->s_mVehiclePool);
 		
 		if (poolAvailable && s_allVehSnapshot.empty() && s_scanIndex == 0 && (ImGui::GetTime() - s_lastSnapTime) > 0.75) {
 			s_allVehSnapshot.clear();
 			if (Safe_ProcessVehiclePool()) {
 				for (auto vehAddr : ***Address::Get()->s_mVehiclePool) {
 					if (vehAddr) s_allVehSnapshot.emplace_back((void*)vehAddr);
 				}
			} else {
				s_allVehSnapshot.clear();
				s_scanIndex = 0;
				s_lastSnapTime = ImGui::GetTime();
			}
 			uint64_t world_lp2 = *(uint64_t*)(Address::Get()->m_sPedFactory);
 			uint64_t lp2 = world_lp2 ? *(uint64_t*)(world_lp2 + 0x8) : 0;
 			GetEntityCoordsFast(lp2, &s_lastLpX, &s_lastLpY, &s_lastLpZ);
 			s_cachedVeh.clear();
 			s_scanIndex = 0;
 			s_lastSnapTime = ImGui::GetTime();
 			if (ServerVehicleList.empty()) { PopulateServerVehicleList(); }
 		}
 		
 		if (!s_allVehSnapshot.empty()) {
 			int processed = 0;
 			while (s_scanIndex < s_allVehSnapshot.size() && processed < s_batchPerFrame) {
 				void* vehAddr = s_allVehSnapshot[s_scanIndex++];
 				if (!vehAddr) { processed++; continue; }
 				
 				unsigned int hash = 0; 
 				if (!Safe_ReadVehicleModelInfo((uint64_t)vehAddr, &hash)) {
 					
 					processed++;
 					continue;
 				}
 				int dist_m = 0; float vx=0,vy=0,vz=0;
 				if (GetEntityCoordsUltraFast((uint64_t)vehAddr, &vx, &vy, &vz)) {
 					float dx=s_lastLpX-vx, dy=s_lastLpY-vy, dz=s_lastLpZ-vz;
 					dist_m = (int)(sqrtf(dx*dx+dy*dy+dz*dz));
 				}
 				if (s_maxDistance > 0 && dist_m > s_maxDistance && dist_m != 0) { processed++; continue; }
 				CachedVehItem item{}; item.addr = vehAddr; item.hash = hash; item.dist_m = dist_m; item.label[0] = '\0';
 				const std::string& base = LookupVehNameByHash(hash);
 				if (dist_m > 0) _snprintf_s(item.label, _countof(item.label), _TRUNCATE, "%s (%dm)", base.c_str(), dist_m); else _snprintf_s(item.label, _countof(item.label), _TRUNCATE, "%s", base.c_str());
 				s_cachedVeh.emplace_back(item);
 				processed++;
 			}
 			
 			if (s_scanIndex >= s_allVehSnapshot.size()) {
 				std::sort(s_cachedVeh.begin(), s_cachedVeh.end(), [](const CachedVehItem& a, const CachedVehItem& b){ return a.dist_m < b.dist_m; });
 				if (s_maxRows > 0 && (int)s_cachedVeh.size() > s_maxRows) s_cachedVeh.resize(s_maxRows);
 				s_allVehSnapshot.clear();
 				s_scanIndex = 0;
 			}
 		}

 		
 		ui::begin_child(sk("Online Vehicles")); {
 			static char onlineVehSearchBuffer[128] = "";
 			
 			
 			ImGui::SetNextItemWidth(-1);
 			ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 10, 10, 210));
 			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(14, 14, 14, 230));
 			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(18, 18, 18, 255));
 			ImGui::InputTextWithHint(sk("##online_veh_search"), sk("Search vehicle..."), onlineVehSearchBuffer, IM_ARRAYSIZE(onlineVehSearchBuffer));
 			ImGui::PopStyleColor(3);

 			
 			ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 10, 210));
 			ImGui::BeginChild(sk("##online_veh_list_child"), ImVec2(-1, 200), false, ImGuiWindowFlags_NoScrollbar);
 			if (ImGui::BeginListBox(sk("##online_veh_list"), ImVec2(-1, -1))) {
 				ImGuiListClipper clipper;
 				clipper.Begin((int)s_cachedVeh.size());
 				while (clipper.Step()) {
 					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
 						const auto& it = s_cachedVeh[i];
 						
 						if (strlen(onlineVehSearchBuffer) > 0 && std::string(it.label).find(onlineVehSearchBuffer) == std::string::npos) continue;
 						
 						bool sel = ((uint64_t)it.addr == globals.index.selected_Vehicle);
 						ImGui::PushID(i);
 						if (ImGui::Selectable(it.label, sel)) {
 							globals.index.selected_Vehicle = (uint64_t)it.addr;
 							globals.index.selectedVehicleIndex = i;
 						}
 						ImGui::PopID();
 					}
 				}
 				ImGui::EndListBox();
 			}
 			ImGui::EndChild();
 			ImGui::PopStyleColor();
 		} ui::end_child();

 		ImGui::SameLine();

 		
 		ui::begin_child(sk("Vehicle Options")); {
 			if (ui::modern_button(sk("Teleport To"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
 				uint64_t localplayer = world ? *(uint64_t*)(world + 0x8) : 0;
 				uint64_t veh = globals.index.selected_Vehicle;
 				if (IsPlayerFullyLoaded() && localplayer && veh) {
 					float vx = *(float*)(veh + 0x90);
 					float vy = *(float*)(veh + 0x94);
 					float vz = *(float*)(veh + 0x98);
 					if (Safe_SetEntityCoordsNoOffset(localplayer, vx, vy, vz + 1.5f, true, false, false, true)) {
 						if (Address::Get()->m_pVelocity) {
 							TryWrite<float>(localplayer + Address::Get()->m_pVelocity + 0x0, 0.f);
 							TryWrite<float>(localplayer + Address::Get()->m_pVelocity + 0x4, 0.f);
 							TryWrite<float>(localplayer + Address::Get()->m_pVelocity + 0x8, 0.f);
 						}
 					}
 				}
 			}
 			if (ui::modern_button(sk("Teleport Into"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
 				uint64_t localplayer = world ? *(uint64_t*)(world + 0x8) : 0;
 				uint64_t veh = globals.index.selected_Vehicle;
 				if (IsPlayerFullyLoaded() && localplayer && veh) {
 					float vx = *(float*)(veh + 0x90);
 					float vy = *(float*)(veh + 0x94);
 					float vz = *(float*)(veh + 0x98);
 					if (Safe_SetEntityCoordsNoOffset(localplayer, vx, vy, vz + 0.3f, true, false, false, true)) {
 						if (Address::Get()->m_pVelocity) {
 							TryWrite<float>(localplayer + Address::Get()->m_pVelocity + 0x0, 0.f);
 							TryWrite<float>(localplayer + Address::Get()->m_pVelocity + 0x4, 0.f);
 							TryWrite<float>(localplayer + Address::Get()->m_pVelocity + 0x8, 0.f);
 						}
 					}
 				}
 			}
 			if (ui::modern_button(sk("Teleport Vehicle Here"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t world = *(uint64_t*)(Address::Get()->m_sPedFactory);
 				uint64_t localplayer = world ? *(uint64_t*)(world + 0x8) : 0;
 				uint64_t veh = globals.index.selected_Vehicle;
 				if (IsPlayerFullyLoaded() && localplayer && veh) {
 					float px = *(float*)(localplayer + 0x90), py = *(float*)(localplayer + 0x94), pz = *(float*)(localplayer + 0x98);
 					if (Safe_SetEntityCoordsNoOffset(veh, px, py, pz + 0.5f, true, false, false, true)) {
 						if (Address::Get()->m_pVelocity) {
 							TryWrite<float>(veh + Address::Get()->m_pVelocity + 0x0, 0.f);
 							TryWrite<float>(veh + Address::Get()->m_pVelocity + 0x4, 0.f);
 							TryWrite<float>(veh + Address::Get()->m_pVelocity + 0x8, 0.f);
 						}
 					}
 					Safe_SetForwardSpeed(veh, 0.0f);
 				}
 			}
 			if (ui::modern_button(sk("Flip Vehicle"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t veh = globals.index.selected_Vehicle;
 				if (IsPlayerFullyLoaded() && veh) {
 					float vx = 0.f, vy = 0.f, vz = 0.f;
 					if (GetEntityCoordsSafe(veh, &vx, &vy, &vz)) {
 						if (Safe_SetEntityCoordsNoOffset(veh, vx, vy, vz + 1.5f, true, false, false, true)) {
 							if (Address::Get()->m_pVelocity) {
 								TryWrite<float>(veh + Address::Get()->m_pVelocity + 0x0, 0.f);
 								TryWrite<float>(veh + Address::Get()->m_pVelocity + 0x4, 0.f);
 								TryWrite<float>(veh + Address::Get()->m_pVelocity + 0x8, 0.f);
 							}
 							Safe_SetForwardSpeed(veh, 0.0f);
 						} else {
 							TryWrite<float>(veh + 0x90, vx);
 							TryWrite<float>(veh + 0x94, vy);
 							TryWrite<float>(veh + 0x98, vz + 2.0f);
 							if (Address::Get()->m_pVelocity) {
 								TryWrite<float>(veh + Address::Get()->m_pVelocity + 0x8, 3.0f);
 							}
 						}
 					}
 				}
 			}
 			
 			ImGui::Separator();
 			
 			if (ui::modern_button(sk("Unlock Doors"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t veh = globals.index.selected_Vehicle;
 				if (IsPlayerFullyLoaded() && veh) {
 					Safe_SetVehicleDoors(veh, false); 
 				}
 			}
 			
 			if (ui::modern_button(sk("Lock Doors"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
 				uint64_t veh = globals.index.selected_Vehicle;
 				if (IsPlayerFullyLoaded() && veh) {
 					Safe_SetVehicleDoors(veh, true); 
 				}
 			}
 		}

 		ui::end_child();
 	});

    ui::add_page(5, []() {
        static bool needRefresh = true;
        static std::vector<std::string> s_files;
        static int s_selected = -1;
        static char s_name[128] = "default";
        static char s_search[128] = "";

        if (needRefresh) {
            s_files = ListConfigFiles();
            needRefresh = false;
            if (s_selected >= (int)s_files.size()) s_selected = -1;
        }

        
        const ImGuiStyle& __cfg_style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, __cfg_style.ChildBorderSize * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::BeginChild(sk("Configs"), ImVec2(300, 0), true); {
            ImGui::SetNextItemWidth(-1);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 10, 10, 210));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(14, 14, 14, 230));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(18, 18, 18, 255));
            ImGui::InputTextWithHint(sk("##cfg_search"), sk("Search config..."), s_search, IM_ARRAYSIZE(s_search));
            ImGui::PopStyleColor(3);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 10, 10, 210));
            ImGui::BeginChild(sk("##cfg_list_child"), ImVec2(-1, 300), false, ImGuiWindowFlags_NoScrollbar);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, __cfg_style.FrameBorderSize * 0.5f);
            if (ImGui::BeginListBox(sk("##cfg_list"), ImVec2(-1, -1))) {
                
                std::vector<int> display_indices;
                display_indices.reserve(s_files.size());
                if (s_search[0] == '\0') {
                    for (int i = 0; i < (int)s_files.size(); ++i) display_indices.push_back(i);
                } else {
                    for (int i = 0; i < (int)s_files.size(); ++i) {
                        if (s_files[i].find(s_search) != std::string::npos) display_indices.push_back(i);
                    }
                }

                ImGuiListClipper clipper;
                clipper.Begin((int)display_indices.size());
                while (clipper.Step()) {
                    for (int idx = clipper.DisplayStart; idx < clipper.DisplayEnd; ++idx) {
                        int i = display_indices[idx];
                        bool sel = (s_selected == i);
                        ImGui::PushID(i);
                        if (ImGui::Selectable(s_files[i].c_str(), sel)) {
                            s_selected = i;
                            strncpy_s(s_name, s_files[i].c_str(), _TRUNCATE);
                        }
                        ImGui::PopID();
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndListBox();
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopStyleColor();
        } ImGui::EndChild();
        ImGui::PopStyleVar(2);

        ImGui::SameLine();

        
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, __cfg_style.ChildBorderSize * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::BeginChild(sk("Config Options"), ImVec2(0, 0), true); {
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText(sk("##cfg_name"), s_name, IM_ARRAYSIZE(s_name));

            ImGui::Separator();

            if (ui::modern_button(sk("Save Config"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
                if (SaveConfig(s_name)) needRefresh = true;
            }

            if (ui::modern_button(sk("Load Config"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
                if (s_selected >= 0 && s_selected < (int)s_files.size()) {
                    LoadConfig(s_files[s_selected]);
                }
                else if (strlen(s_name) > 0) {
                    LoadConfig(s_name);
                }
            }

            if (ui::modern_button(sk("Delete Config"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
                std::string target;
                if (s_selected >= 0 && s_selected < (int)s_files.size()) target = s_files[s_selected];
                else if (strlen(s_name) > 0) target = s_name;
                if (!target.empty()) {
                    auto p = GetConfigsDir() / (target + ".json");
                    std::error_code ec; std::filesystem::remove(p, ec);
                    needRefresh = true;
                    s_selected = -1;
                }
            }

            ImGui::Separator();

            if (ui::modern_button(sk("Refresh List"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
                needRefresh = true;
            }
            if (ui::modern_button(sk("Open Directory"), ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()))) {
                OpenConfigsDirInExplorer();
            }
        } ImGui::EndChild();
        ImGui::PopStyleVar(2);
        });




}


static void RenderPageSEH()
{
	ImGuiContext& g = *GImGui;
	const int styleVarStackSize = g.StyleVarStack.Size;
	const int colorStackSize = g.ColorStack.Size;
	const int fontStackSize = g.FontStack.Size;
	const int groupStackSize = g.GroupStack.Size;
	const int windowStackSize = g.CurrentWindowStack.Size;
	__try {
		ui::render_page();
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		while (g.StyleVarStack.Size > styleVarStackSize)
			ImGui::PopStyleVar();
		while (g.ColorStack.Size > colorStackSize)
			ImGui::PopStyleColor();
		while (g.FontStack.Size > fontStackSize)
			ImGui::PopFont();
		while (g.GroupStack.Size > groupStackSize)
			ImGui::EndGroup();
		while (g.CurrentWindowStack.Size > windowStackSize) {
			ImGuiWindow* window = g.CurrentWindow;
			if (window && (window->Flags & ImGuiWindowFlags_ChildWindow))
				ImGui::EndChild();
			else
				ImGui::End();
		}
		Menu::RecoverImGuiStack();
	}
}

static void RenderLoadingScreen() {
	ImDrawList* draw = ImGui::GetForegroundDrawList();
	ImVec2 disp = ImGui::GetIO().DisplaySize;
	double now = ImGui::GetTime();

	if (globals.menu.loading_start_time == 0.0)
		globals.menu.loading_start_time = now;

	double elapsed = now - globals.menu.loading_start_time;
	float progress = ImClamp((float)(elapsed / 2.5), 0.0f, 1.0f);
	globals.menu.loading_progress = progress;

	if (progress >= 1.0f) {
		globals.menu.loading_done = true;
		return;
	}

	float alpha = 1.0f;
	if (progress > 0.85f)
		alpha = 1.0f - ((progress - 0.85f) / 0.15f);

	draw->AddRectFilled(ImVec2(0, 0), disp, ImGui::GetColorU32(ImVec4(0.03f, 0.03f, 0.04f, alpha)));

	ImVec2 center(disp.x * 0.5f, disp.y * 0.5f);
	float time = (float)now;

	float radius = 20.0f;
	int numPts = 32;
	for (int i = 0; i < numPts; i++) {
		float a1 = time * 3.0f + (6.2831f * i / numPts);
		float a2 = time * 3.0f + (6.2831f * (i + 1) / numPts);
		ImVec2 p1(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
		ImVec2 p2(center.x + cosf(a2) * radius, center.y + sinf(a2) * radius);
		float seg_alpha = (float)i / numPts;
		draw->AddLine(p1, p2, ImGui::GetColorU32(ImVec4(0.40f, 0.42f, 0.96f, seg_alpha * alpha)), 1.5f);
	}

	float barW = 120.0f;
	float barH = 2.0f;
	ImVec2 barMin(center.x - barW * 0.5f, center.y + 38.0f);
	ImVec2 barMax(barMin.x + barW, barMin.y + barH);
	draw->AddRectFilled(barMin, barMax, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.05f * alpha)), 1.0f);
	ImVec2 fillMax(barMin.x + barW * progress, barMax.y);
	draw->AddRectFilled(barMin, fillMax, ImGui::GetColorU32(ImVec4(0.40f, 0.42f, 0.96f, 0.8f * alpha)), 1.0f);
}

void Menu::Load() {
	
	TickLuaInjector();
	
	if (!globals.menu.loading_done) {
		RenderLoadingScreen();
		return;
	}

	static float menu_anim = 0.0f;
	static float bounce_anim = 0.0f;
	float dt = ImGui::GetIO().DeltaTime;
	float target = globals.menu.opened ? 1.0f : 0.0f;
 	
 	
 	menu_anim = ImLerp(menu_anim, target, 12.0f * dt);
 	
 	
 	if (target > 0.5f && menu_anim > 0.85f) {
 		bounce_anim = ImLerp(bounce_anim, 1.0f, 8.0f * dt);
 	} else {
 		bounce_anim = ImLerp(bounce_anim, 0.0f, 15.0f * dt);
 	}
 	
 	
 	float cubic_out = 1.0f - (1.0f - menu_anim) * (1.0f - menu_anim) * (1.0f - menu_anim);
 	float bounce_offset = bounce_anim * 0.08f * sinf(bounce_anim * 3.14159f * 2.0f);
 	float eased_menu_anim = ImClamp(cubic_out + bounce_offset, 0.0f, 1.0f);

 	ImGui::GetIO().MouseDrawCursor = globals.menu.opened;
 	if (menu_anim > 0.01f) {
 		
 		ImDrawList* __bg = ImGui::GetBackgroundDrawList();
 		ImVec2 __disp = ImGui::GetIO().DisplaySize;
 		__bg->AddRectFilled(ImVec2(0, 0), __disp, ImGui::GetColorU32(ImVec4(0, 0, 0, 0.25f * eased_menu_anim)));

 		
 		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, eased_menu_anim);
 		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
 		ImGui::Begin(sk("Settings"), 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
 		{
 			ImGui::PopStyleVar();
 			ImGui::SetWindowSize(ui::size);

 			
 			{
 				const ImGuiStyle& style = ImGui::GetStyle();
 				ImVec2 s = ImVec2(ImGui::GetWindowSize().x - style.WindowPadding.x * 2, ImGui::GetWindowSize().y - style.WindowPadding.y * 2);
 				ImVec2 p = ImVec2(ImGui::GetWindowPos().x + style.WindowPadding.x, ImGui::GetWindowPos().y + style.WindowPadding.y);
 				ImDrawList* draw = ImGui::GetWindowDrawList();
 				ImU32 main_col = ImGui::GetColorU32(ImGuiCol_Scheme);

 				
 				draw->PushClipRect(ImVec2(p.x, p.y), ImVec2(p.x + s.x * eased_menu_anim, p.y + s.y), true);

 				
				draw->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + s.x, p.y + s.y), ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.06f, 0.98f)), 10.0f);
				draw->AddRect(ImVec2(p.x, p.y), ImVec2(p.x + s.x, p.y + s.y), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.06f)), 10.0f);

				const float header_height = 38.0f;
				draw->AddLine(ImVec2(p.x, p.y + header_height), ImVec2(p.x + s.x, p.y + header_height), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)));
				draw->AddLine(ImVec2(p.x, p.y + s.y - 24.0f), ImVec2(p.x + s.x, p.y + s.y - 24.0f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)));

				draw->AddLine(ImVec2(p.x + 94.0f, p.y + header_height), ImVec2(p.x + 94.0f, p.y + s.y - 24.0f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)));

				draw->AddText(ImVec2(p.x + 14.0f, p.y + 12.f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.6f)), sk("NIDEV").decrypt());
				draw->AddText(ImVec2(p.x + 12.0f, p.y + s.y - 18.f), ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.25f)), sk("beta").decrypt());

 				
 				draw->PopClipRect();
 			}
  
 			
			if (!HasSafeSessionContext())
				s_luaTabConfirmed = false;

			{
				const float tab_start_x = 70.0f;
				const float tab_area_w = ui::size.x - tab_start_x;
				const int tab_count = (int)ui::tabs.size();
				const float per_tab_w = tab_area_w / (float)tab_count;
				ImGui::SetCursorPos(ImVec2(tab_start_x, 4.0f));
				ImGui::BeginGroup();
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
				for (int i = 0; i < tab_count; ++i) {
					ui::tab_width = per_tab_w;
					ui::tab(i);
					if (i + 1 < tab_count)
						ImGui::SameLine();
				}
				ImGui::PopStyleVar();
				ImGui::EndGroup();
			}

 			
 			if (!ui::tabs[ui::cur_page].m_subtabs.empty()) {
 				for (int i = 0; i < ui::tabs[ui::cur_page].m_subtabs.size(); ++i) {
					ImGui::SetCursorPosY(72.0f + i * 28.0f);
 					ImGui::SetCursorPosX(30.0f);
 					ui::subtab(i);
 				}
 			}

 			
			ImGui::SetCursorPosY(56.0f);
 			ImGui::SetCursorPosX(122.0f);
 			
 			{
 				float slide_x = (1.0f - eased_menu_anim) * 24.0f;
 				float slide_y = (1.0f - (ui::content_anim * ui::content_anim2)) * 8.0f;
 				ImVec2 cur = ImGui::GetCursorPos();
 				ImGui::SetCursorPos(ImVec2(cur.x + slide_x, cur.y + slide_y));
 			}
  
 			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 20, 20 });
 			
 			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, eased_menu_anim * ui::content_anim * ui::content_anim2);
			ImGui::BeginChild(sk("main"), { 0, -1 }, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
 			{
 				
 				
 				ImDrawList* cd = ImGui::GetWindowDrawList();
 				ImVec2 cpos = ImGui::GetWindowPos();
 				ImVec2 csize = ImGui::GetWindowSize();
 				float reveal = ImSaturate(eased_menu_anim * (0.3f + 0.7f * ui::content_anim) * (0.3f + 0.7f * ui::content_anim2));
 				cd->PushClipRect(cpos, ImVec2(cpos.x + csize.x * reveal, cpos.y + csize.y), true);

				RenderPageSEH();

				cd->PopClipRect();
 			}
 			ImGui::EndChild();
 			ImGui::PopStyleVar(2);
 		}
 		ImGui::End();
 		ImGui::PopStyleVar(); 

 		ui::handle_alpha_anim();
 	}

 	
 	if (globals.menu_settings.crosshair) {
 		ImDrawList* overlay = ImGui::GetForegroundDrawList();
 		ImVec2 disp = ImGui::GetIO().DisplaySize;
 		ImVec2 center(disp.x * 0.5f, disp.y * 0.5f);

 		float size = globals.menu_settings.crosshair_size;           
 		float gap = size * 0.35f;                                    
 		float length = size;                                         
 		float thickness = globals.menu_settings.crosshair_thickness; 
 		ImU32 col = ImGui::GetColorU32(ImVec4(
 			globals.menu_settings.crosshair_color[0],
 			globals.menu_settings.crosshair_color[1],
 			globals.menu_settings.crosshair_color[2],
 			globals.menu_settings.crosshair_color[3]));

 		
 		overlay->AddLine(ImVec2(center.x - gap - length, center.y), ImVec2(center.x - gap, center.y), col, thickness);
 		overlay->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + length, center.y), col, thickness);
 		
 		overlay->AddLine(ImVec2(center.x, center.y - gap - length), ImVec2(center.x, center.y - gap), col, thickness);
 		overlay->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + length), col, thickness);
 	}

 	
 	if (globals.menu_settings.spectator_list) {
 		static double s_lastScan = 0.0;
 		static std::unordered_map<unsigned long long, std::pair<double, std::string>> s_seenUntil; 
 		static std::vector<std::string> s_display;

 		double now = ImGui::GetTime();
		if (!HasSafeSessionContext()) {
			s_seenUntil.clear();
			s_display.clear();
			return;
		}
 		if ((now - s_lastScan) > 0.25) { 
 			s_lastScan = now;
 			
 			for (auto it = s_seenUntil.begin(); it != s_seenUntil.end();) {
 				if (it->second.first < now) it = s_seenUntil.erase(it); else ++it;
 			}
 			
			uint64_t world_lp = 0;
			TryRead<uint64_t>(Address::Get()->m_sPedFactory, &world_lp);
			uint64_t lp = 0;
			if (world_lp) TryRead<uint64_t>(world_lp + 0x8, &lp);
 			float lpx=0.f, lpy=0.f, lpz=0.f;
 			bool have_local = GetEntityCoordsFast(lp, &lpx, &lpy, &lpz);
 			if (have_local && Address::Get()->s_mPedPool) {
				for (auto cPed : **Address::Get()->s_mPedPool) {
 					if (!cPed || (uint64_t)cPed == lp) continue;
					uint32_t ped_type = 0;
					if (!TryRead<uint32_t>((uint64_t)cPed + Address::Get()->m_pEntityType, &ped_type) || !ped_type) continue;
 					ped_type = ped_type << 11 >> 25;
 					if (ped_type != 2) continue; 
 					float tx=0,ty=0,tz=0;
 					if (!GetEntityCoordsFast((uint64_t)cPed, &tx, &ty, &tz)) continue;
 					float dx = lpx - tx, dy = lpy - ty, dz = lpz - tz;
 					float horiz = sqrtf(dx*dx + dy*dy);
 					
 					if (horiz < 4.0f && dz > 2.0f && dz < 25.0f) {
 						
 						std::string nameStr;
						uint64_t playerinfo = 0;
						TryRead<uint64_t>((uint64_t)cPed + Address::Get()->m_pPlayerInfo, &playerinfo);
 						unsigned long long netid64 = 0ULL;
 						if (playerinfo) {
							nameStr = Address::Get()->GetPlayerNameFromInfo(playerinfo);
							uint64_t netid = 0;
							TryRead<uint64_t>(playerinfo + Address::Get()->m_pNetid, &netid);
 							netid64 = (unsigned long long)netid;
							if (nameStr.empty() || nameStr == "NPC") {
 								std::string name = Address::Get()->GetPlayerNameByNetId(netid);
 								if (name != "NPC" && !name.empty()) nameStr = name;
							}
 						}
 						if (nameStr.empty()) nameStr = "Player";
 						
 						s_seenUntil[netid64 ? netid64 : (unsigned long long)(uintptr_t)cPed] = { now + 2.0, nameStr }; 
 					}
 				}
 			}
 			
 			s_display.clear();
 			for (const auto& kv : s_seenUntil) {
 				s_display.emplace_back(kv.second.second);
 			}
 			std::sort(s_display.begin(), s_display.end());
 		}

 		if (!s_display.empty()) {
 			ImDrawList* dl = ImGui::GetForegroundDrawList();
 			ImVec2 disp = ImGui::GetIO().DisplaySize;
 			
 			float padding = 8.f;
 			float line_h = ImGui::GetTextLineHeight();
 			float max_w = ImGui::CalcTextSize(sk("Spectators").decrypt()).x;
 			for (auto& n : s_display) max_w = ImMax(max_w, ImGui::CalcTextSize(n.c_str()).x);
 			float w = max_w + padding * 2.f;
 			float h = (1 + (float)s_display.size()) * line_h + padding * 2.f; 
 			ImVec2 pos(disp.x - w - 12.f, 12.f);
 			ImU32 bg = ImGui::GetColorU32(ImVec4(0,0,0,0.35f));
 			ImU32 bord = ImGui::GetColorU32(ImVec4(1,1,1,0.15f));
 			dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), bg, 6.f);
 			dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), bord, 6.f);
 			
 			dl->AddText(ImVec2(pos.x + padding, pos.y + padding), ImGui::GetColorU32(ImVec4(1,1,1,0.9f)), sk("Spectators").decrypt());
 			float y = pos.y + padding + line_h;
 			for (auto& n : s_display) {
 				dl->AddText(ImVec2(pos.x + padding, y), ImGui::GetColorU32(ImVec4(1,1,1,0.9f)), n.c_str());
 				y += line_h;
 			}
 		}
 	}
 }


static std::vector<std::string> s_luaQueue;
static double s_luaLastEnqueue = 0.0;
static bool s_luaInjectBusy = false;
static double s_luaBusyUntil = 0.0;

static inline void SafeLuaEnqueueScript(const std::string& script)
{
    if (script.empty()) return;
    s_luaQueue.emplace_back(script);
    s_luaLastEnqueue = ImGui::GetTime();
    char msg[256];
    _snprintf_s(msg, _countof(msg), _TRUNCATE, "Script queued for execution (%zu chars)", script.size());
    AppendLuaLog(msg);
}

static void TickLuaInjector()
{
    double now = ImGui::GetTime();
    if (s_luaInjectBusy && now >= s_luaBusyUntil) { s_luaInjectBusy = false; s_luaBusyUntil = 0.0; }
    if (s_luaInjectBusy) return;
    if (s_luaQueue.empty()) return;
    if (!IsLuaTabAvailable()) {
        AppendLuaLog("Lua queue cleared: no active server session.");
        s_luaQueue.clear();
        return;
    }

    bool ready = ((now - s_luaLastEnqueue) > 0.12) || (s_luaQueue.size() >= 5);
    if (!ready) return;
    
    char queue_info[256];
    _snprintf_s(queue_info, _countof(queue_info), _TRUNCATE, "Processing Lua queue: %zu items, last enqueue %.3fs ago", s_luaQueue.size(), now - s_luaLastEnqueue);
    AppendLuaLog(queue_info);

    std::string batch;
    size_t total = 0;
    for (auto& s : s_luaQueue) total += s.size() + 2;
    batch.reserve(total);
    for (size_t i = 0; i < s_luaQueue.size(); ++i) {
        batch += s_luaQueue[i];
        batch += ";\n";
    }

    char batch_info[256];
    _snprintf_s(batch_info, _countof(batch_info), _TRUNCATE, "Executing batch: %zu items, %zu chars", s_luaQueue.size(), batch.size());
    AppendLuaLog(batch_info);
    
    if (SafeLuaExecScript(batch)) {
        char msg[128]; _snprintf_s(msg, _TRUNCATE, "Batch executed %zu item(s).", s_luaQueue.size());
        AppendLuaLog(msg);
        s_luaQueue.clear();
        s_luaInjectBusy = true;
        s_luaBusyUntil = now + 0.75;
    } else {
        AppendLuaLog("Failed to execute Lua batch");
    }
}

static void OpenConfigsDirInExplorer()
{
	std::filesystem::path dir = GetConfigsDir();
	std::wstring w = dir.wstring();
	ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
