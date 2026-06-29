#pragma once

#include "includes.h"
#include <unordered_set>

inline const char* weapon_list[]{
	"Dagger", "Bat", "Knife", "Machete",
	"Pistol", "Pistol MK2", "Combat Pistol", "AP Pistol",
	"Stungun", "Pistol 50", "SNS PISTOL", "SNS Pistol MK2",
	"Heavy Pistol", "Vintage Pisol", "Flare Gun", "Marksman Pistol",
	"Revolver", "Revolver MK2", "Double Action", "Micro Smg",
	"Smg", "Smg MK2", "Assault Smg", "Combat PDW",
	"Machine Pistol", "Mini Smg", "Pump Shotgun", "Pump Shotgun MK2",
	"Sawnoff Shotgun", "Assault Shotgun", "Bullpup Shotgun", "Musket",
	"Heavy Shotgun", "DB Shotgun", "Auto Shotgun", "Assault Rifle",
	"Assault Rifle MK2", "Carbine Rifle", "Carbine Rifle MK2", "Advanced Rifle",
	"Special Carbine", "Special Carbine MK2", "Bullpup Rifle", "Bullpup Rifle MK2",
	"Compact Rifle", "Machine Gun", "Combat MG", "Combat MG MK2",
	"GUSENBERG", "Sniper Rifle", "AWP", "AWP MK2",
	"Marksman Rifle", "Marksman Rifle MK2", "RPG", "Grenade Launcher",
	"MiniGun", "FireWork", "RailGun", "Homing Launcher",
	"Compact Launcher", "Grenade", "BZGAS", "Smoke Grenade",
	"Flare", "Molotov", "Sticky BOMB", "Prox Mine",
	"SnowBall", "Pipe Bomb", "Ball", "Petrol Can",
	"Fire Extinguisher", "Parachute"
};

class cGlobals {
public:
	class cIndex
	{
	public:
		int selectedPlayerIndex = -1;
		uint64_t selected_player = 0;
		int selectedVehicleIndex = -1;
		uint64_t selected_Vehicle = 0;
	};
	cIndex index;

	class cAimbot {
	public:
		bool active = false;

		bool draw_fov = false;
		bool point_players = false;
		bool point_npcs = false;
		bool point_dead = false;
		bool point_animals = false;

		int max_dist = 150;
		int smooth = 5;
		int fov = 5;
		int bone = 0;
		bool closest_bone = false;
		bool draw_aim_line = false;
		bool avoid_repeats = false;
		bool visible_only = false;
		int target_priority = 0; 


		bool silent_aim = false;
		bool magic_aim = false;

		int silent_fov = 5;
		int silent_bone = 0;
		int silent_miss_chance = 0; 

		c_key hotkey;
		
		c_key silent_hotkey;

		std::vector<const char*> bones_list{ "Head", "Neck", "Pelvis", "Left Foot", "Right Foot", "Right Hand", "Left Hand" };
		std::vector<const char*> aim_priorities{ "FOV", "Distance", "Health" };
	};
	cAimbot aimbot;

	class cVisuals
	{
	public:
		bool activate = false;
		
		std::vector<const char*> boxstyles{ "Static", "Filled Box", "Corner" };
		bool box = false;
		int boxstyle = 1;
		int box_thickness = 1;
		
		std::vector<const char*> skeleton_types{ "Simple", "Complex" };
		bool skeleton = false;
		int skeleton_thickness = 1;
		int skeleton_type = 1;
		
		bool armorbar = false;
		std::vector<const char*> armorpositions{ "Side", "Bottom", "Top" };
		int armorposition = 1;
		
		bool weapon_name = false;
		
		bool healthbar = false;
		std::vector<const char*> healthpositions{ "Side", "Bottom", "Top" };
		int healthposition = 1;
		
		bool names = false;
		
		bool snapline = false;
		int snapline_thickness = 1;
		
		bool distance = false;

		bool show_dead = false;
		bool show_npcs = false;
		bool show_self = false;
		bool show_animals = false;
		bool show_friends = false;

		bool watermark = false;
		bool crosshair = false;
		int view_distance = 200;
		int text_size = 14;

		c_key master_switch;

		float boxcolor[4]{ 1.f, 1.f, 1.f, 1.f };
		float namecolor[4]{ 1.f, 1.f, 1.f, 1.f };
		float skeleton_color[4]{ 1.f, 1.f, 1.f, 1.f };
		float weapon_color[4]{ 1.f, 1.f, 1.f, 1.f };
		float snapline_color[4]{ 1.f, 1.f, 1.f, 1.f };
		float distance_color[4]{ 1.f, 1.f, 1.f, 1.f };
		float fov_color[4]{ 1.f, 1.f, 1.f, 1.f };
		float preview_target_col[4]{ 1.f, 1.f, 1.f, 1.f };
		float silent_fov_color[4]{ 1.f, 1.f, 1.f, 1.f };
		float silent_preview_target_col[4]{ 1.f, 1.f, 1.f, 1.f };
		float triggerbot_preview_target_col[4]{ 1.f, 1.f, 1.f, 1.f };

		
		int player_max_display = 32;   
		int npc_max_display = 64;      
		int skeleton_max_display = 32; 
		int skeleton_max_distance = 150; 

		
		bool veh_activate = false;
		bool veh_box = false;
		int veh_boxstyle = 1;
		int veh_box_thickness = 1;
		bool veh_names = false;
		bool veh_distance = false;
		int veh_view_distance = 500; 
		bool veh_snapline = false;
		int veh_snapline_thickness = 1;
		int veh_label_pos = 0; 
		float veh_boxcolor[4]{ 1.f, 1.f, 1.f, 1.f };
		float veh_namecolor[4]{ 1.f, 1.f, 1.f, 1.f };
		
		
		int veh_max_display = 50; 
		bool veh_fade_with_distance = true;
		bool veh_label_bg = false;
		bool veh_velocity_line = false;
		int veh_text_size = 14;
	};
	cVisuals visuals;

	
	struct TeleportLocation {
		const char* name;
		float x, y, z;
	};

	class cSelf
	{
	public:
		bool nocol = false;
		bool semigodmode = false;
		bool godmode = false;
		bool noclip = false;
		bool outfit = false;
		
		bool Invisible = false;
		c_key invisible_key;
		
		bool revive_request = false;
		c_key revive_key;

		
		bool desync = false;
		c_key desync_key;

		
		bool no_ragdoll = false;
		bool no_fall = false;
		int fall_speed_limit = 5; 

		
		bool slow_motion = false;
		float slow_motion_factor = 0.5f; 

		
		bool auto_heal = false;
		int min_health = 75; 
		bool auto_armor = false;
		bool suicide_request = false; 
		bool heal_request = false;
		bool armor_request = false;
		bool freecam = false;
		int freecam_speed = 1;
		c_key freecam_key;
		bool freecam_hud = true;
		int freecam_current_mode = 0; 
		
		bool freecam_teleport_on_disable = false;

		bool spectate = false;
		uint64_t spectate_target = 0;

		
		int selected_teleport_index = 0;

		bool super_jump = false;
		bool beast_jump = false;

		bool custom_altitude = false;
		bool custom_fov = false;
		float custom_altitude_value = 1.f;
		float custom_fov_value = 1.f;

		c_key hotkey_noclip;
		int noclip_speed = 1;
	};
	cSelf self;

	class cVehicle
	{
	public:
		bool repair = false;
		bool shift_boost = false;
		bool godmode = false;
		bool rocket_boost = false;
		bool modify_gravity = false;
		bool apply_color_change = false;

		bool spawn_vehicle = false;
		int spawn_vehicle_hash = 1475773103;
		bool spawn_inside = true;
		int shift_boost_value = 1;
		int gravity_value = 1;
		ImColor primary_vehicle_color = ImColor(255, 255, 255, 255);
		ImColor secondary_vehicle_color = ImColor(255, 255, 255, 255);

		char plate_text[16] = "noname"; 
		bool set_plate = false;
	};
	cVehicle vehicle;

	class cWeapon
	{
	public:
		bool weapon_enabled = false;
		bool no_recoil = false;
		bool no_reload = false;
		bool no_spread = false;

		bool explosiveammo = false;
		bool fire_ammo = false;
		bool spawn_weapon = false;

		bool range_multiplier = false;
		bool freeze_ammo = false;
		bool damage_boost = false;
		bool tp_to_bullet = false;
		bool bullet_traces = false;
		
		bool infinite_ammo = false;
		int spawn_ammo_count = 0; 
		int weapon_range = 1000;
		int weapon_damage = 1000;
		int weapon_spawn_index = -1;
		bool weapon_spawn = false;

		ImColor bullet_traces_col = ImColor(255, 255, 255);

	};
	cWeapon weapon;



	class cOnlineVehicles
	{
	public:
		int max_distance = 500; 
		size_t max_display_count = 50; 
		bool sort_by_distance = true; 
	};
	cOnlineVehicles online_vehicles;

	class cMenu {
	public:
		c_key hotkey{ VK_INSERT };
		bool opened = true;
		bool unload_requested = false;
		bool unloaded = false;
		bool loading_done = false;
		float loading_progress = 0.0f;
		double loading_start_time = 0.0;
	};
	cMenu menu;

	char player_list_search[100];
	

	
	struct cMenuSettings {
		bool block_input = true; 
		bool crosshair = false; 
		float crosshair_color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; 
		float crosshair_size = 5.0f; 
		float crosshair_thickness = 1.0f; 
		bool spectator_list = false; 
	};
	cMenuSettings menu_settings;

};
inline cGlobals globals;
inline std::unordered_set<uint64_t> g_friend_netids;