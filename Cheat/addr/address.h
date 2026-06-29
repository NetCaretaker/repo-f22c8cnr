#pragma once

#include "../../Main/includes/includes.h"
typedef uint64_t(*add_weapon_t)(uint64_t pedinventory, uint64_t uWeaponNameHash);
typedef uint64_t(*explode_vehicule)(uint64_t vehicule, bool isAudible, bool isInvisible);
typedef uint64_t(*spawn_vehicle)(uint64_t vehicule, float x, float y, float z, float heading, bool IsNetworked, bool proot);
typedef uint64_t(*request_model_t)(uint64_t Hash);

typedef uint64_t(*set_model_no_longer_needed_t)(uint64_t Hash);

typedef bool(*has_model_loaded_t)(uint64_t Hash);

typedef uint64_t(*is_model_in_cd_image_t)(uint64_t Hash);

typedef uint64_t(*create_vehicle_t)(uint64_t modelHash,PVector3 caca, float heading, bool isNetwork, bool netMissionEntity);
using pizza_to_spaghetti_t = int32_t(*)(intptr_t pointer); 
typedef const char* (*get_player_name_from_playerinfo_t)(uint64_t playerinfo);


typedef uint64_t(*repair_vehicle_t)(uint64_t vehicle);
typedef uint64_t(*set_plate_t)(uint64_t vehicle, const char* plate);
typedef uint64_t(*Set_forward_speed_t)(uint64_t vehicle, float speed);
typedef uint64_t(*SetEntityCoordsNoOffset_t)(uint64_t entity, float x, float y, float z, bool alive, bool deadFlag, bool ragdollFlag, bool clearArea);
typedef uint64_t(*random_outfit2_t)(uint64_t ped);
typedef uint64_t(*random_outfit_t)(uint64_t ped);
typedef uint64_t(*set_ped_random_component_t)(uint64_t ped);
typedef uint64_t(*set_ped_random_props_t)(uint64_t ped);

typedef void(*spectate_t)(bool toggle, int pedHandle);
typedef void(*add_explosion_t)(float x, float y, float z, int type, float damageScale, bool isAudible, bool isInvisible, float cameraShake);
typedef void(*raycast_t)(PVector3* p1, PVector3* p2, int p3, uint64_t p4, int p5);
typedef void(*task_shoot_gun_at_coord_t)(uint64_t ped, float x, float y, float z, int duration, uint32_t firingPattern, bool unk);
typedef void(*task_shoot_at_entity_t)(uint64_t ped, uint64_t target, int duration, uint32_t firingPattern);

namespace MemoryAddress {
   inline  uintptr_t s_OrigAccurate;
   inline  uintptr_t s_OrigWeapon;
   inline  uintptr_t s_Invisible;
   inline add_weapon_t add_weapon = NULL;
   inline explode_vehicule explose_vehicule = NULL;
   inline spawn_vehicle spawn_vehicule = NULL;

   inline request_model_t request_model;

   inline set_model_no_longer_needed_t set_model_no_longer_needed;

   inline has_model_loaded_t has_model_loaded;

   inline is_model_in_cd_image_t is_model_in_cd_image;

   inline create_vehicle_t create_vehicle;

   inline pizza_to_spaghetti_t pointer_to_handle;

   
   inline repair_vehicle_t repair_vehicle;
   inline set_plate_t set_plate;
   inline Set_forward_speed_t Set_forward_speed;
   inline SetEntityCoordsNoOffset_t SetEntityCoordsNoOffset;
   inline random_outfit2_t random_outfit2;
   inline random_outfit_t random_outfit;
   inline set_ped_random_component_t set_ped_random_component;
   inline set_ped_random_props_t set_ped_random_props;
   inline spectate_t spectate = NULL;
   inline add_explosion_t add_explosion = NULL;

   
   inline uintptr_t g_world2screen = 0;
   inline uintptr_t g_bonemask = 0;
   inline uintptr_t NetworkRequestControlOfEntity_addr = 0;
   inline uintptr_t ClearPedTask_addr = 0;
   inline uintptr_t SetPedAmmo_addr = 0;
   inline uintptr_t GET_ENTITY_BONE_INDEX_BY_NAME_addr = 0;
   inline uintptr_t GET_WORLD_POSITION_OF_ENTITY_BONE_addr = 0;
   inline uintptr_t give_weapon_to_ped_addr = 0;
   inline uintptr_t no_reload_addr = 0;
   inline uintptr_t WayPointRead = 0; 
   inline uintptr_t ClearPedBlood_addr = 0;
   inline uintptr_t oGameThread_addr = 0;

   
   inline raycast_t raycast = nullptr;
   inline task_shoot_gun_at_coord_t task_shoot_gun_at_coord = nullptr;
   inline task_shoot_at_entity_t task_shoot_at_entity = nullptr;
   inline uintptr_t MagicBulletsPatch = 0;
   inline uintptr_t ArmsKinematics = 0;
   inline uintptr_t LegsKinematics = 0;
   inline uintptr_t SilentAim = 0;
   inline uintptr_t InfiniteCombatRoll = 0;
   inline uintptr_t InfiniteAmmo0 = 0;
   inline uintptr_t InfiniteAmmo1 = 0;
   inline uintptr_t AimCPedPatternResult = 0;
   inline uintptr_t SpectatorBypass = 0;
   inline uintptr_t SpectatorFlagPtr1 = 0;
   inline uintptr_t SpectatorFlagPtr2 = 0;
   inline uintptr_t SpectatorBypassTestJnz = 0;
   inline get_player_name_from_playerinfo_t get_player_name_from_playerinfo = nullptr;

}
class Address {
    template<typename T>
    class pool_iterator {
    public:
        T* pool = nullptr;
        uint32_t index = 0;

        explicit pool_iterator(T* pool, uint32_t index = 0) : pool(pool), index(index) {}

        pool_iterator& operator++() {
            while (++index < pool->m_size) {
                if (pool->is_valid(index)) {
                    return *this;
                }
            }

            index = pool->m_size;
            return *this;
        }

        bool operator!=(const pool_iterator& other) const {
            return this->index != other.index;
        }

        auto operator*() const {
            return pool->get_address(index);
        }
    };

    template<typename T>
    class PoolUtils {
    public:
        pool_iterator<T> begin() {
            return ++pool_iterator<T>(static_cast<T*>(this), -1);
        }

        pool_iterator<T> end() {
            return pool_iterator<T>(static_cast<T*>(this), static_cast<T*>(this)->m_size);
        }
    };

public:
    class VehiclePool : public PoolUtils<VehiclePool> {
    public:
        UINT64* m_pool_address;
        UINT32 m_size;
        char _Padding2[36];
        UINT32* m_bit_array;
        char _Padding3[40];
        UINT32 m_item_count;

        inline bool is_valid(UINT32 i) const {
            return (m_bit_array[i >> 5] >> (i & 0x1F)) & 1;
        }

        inline UINT64 get_address(UINT32 i) const {
            return m_pool_address[i];
        }
    };

    class PedPool : public PoolUtils<PedPool> {
    public:
        UINT64 m_pool_address; 
        BYTE* m_bit_array;     
        UINT32 m_size;         
        UINT32 m_item_size;    
        UINT32 m_pad[2];       
        UINT32 m_item_count;   

        inline bool is_valid(UINT32 i) const {
            return ma_(i) != 0;
        }

        inline UINT64 get_address(UINT32 i) const {
            return (m_pool_address + static_cast<uint64_t>(i) * m_item_size) & ma_(i);
        }

        inline int get_item_count() const {
            return (4 * m_item_count) >> 2;
        }

    private:
        inline long long ma_(UINT32 i) const {
            long long num1 = m_bit_array[i] & 0x80;
            return ~((num1 | -num1) >> 63);
        }
    };

    class GenericPool : public PoolUtils<GenericPool> {
    public:
        UINT64 m_pool_address; 
        BYTE* m_bit_array;     
        UINT32 m_size;         
        UINT32 m_item_size;    
        UINT32 m_pad[2];       
        UINT32 m_item_count;   

        inline bool is_valid(UINT32 i) {
            return ma_(i) != 0;
        }

        inline UINT64 get_address(UINT32 i) {
            return ma_(i) & (m_pool_address + (uint64_t)i * m_item_size);
        }

        inline int get_item_count() {
            return (4 * m_item_count) >> 2;
        }

    private:
        inline long long ma_(UINT32 i) {
            long long num1 = m_bit_array[i] & 0x80;
            return ~((num1 | -num1) >> 63);
        }
    };
public:
	static Address* Get() {
		if (s_pSingleton == nullptr)
			s_pSingleton = new Address;
		return s_pSingleton;
	}




	static Address* s_pSingleton;

	uint64_t m_sPedFactory;

	uint64_t s_pViewAngles;
	uint64_t s_pViewPort;
	uint64_t s_pSwapChain;
	uint64_t s_pBulletPointer;

	GenericPool** s_mPedPool;
    VehiclePool*** s_mVehiclePool;

	DWORD m_pBoneOffset;

    DWORD m_pArmor;
    DWORD m_pEntityType;
    DWORD m_pWeaponManager;
    DWORD m_pPlayerInfo;
    DWORD m_pRecoil;
    DWORD m_pSpread;
    DWORD m_pRange;
    DWORD m_pReloadMult;
    DWORD m_pDoorstatus;
    DWORD m_pEnghealth;
    DWORD m_pNetid;
    DWORD m_pPlayerName;
    DWORD m_pVehMgr;
    DWORD m_pHandlingData;
    DWORD m_pGravity;
    DWORD m_pVelocity;
    DWORD m_pPedTask;
    DWORD m_pFrameFlags;
    DWORD m_pConfigFlags;
    uint64_t freecam_nop;

    DWORD64 dwThreadCollectionPtr;
    DWORD64 tickFuncPtr;
    UINT32 activeThreadTlsOffset;
    int version;
    std::string GetPlayerNameByNetId(int netid);
    std::string GetPlayerNameFromInfo(uint64_t playerinfo);
    void GetPlayerNameInternal(int netid, char* outName, size_t outSize);
	
	void Load();
    
    
    inline void EnumerateVehicles(std::vector<uint64_t>& out)
    {
        out.clear();
        if (!s_mVehiclePool) return;
        VehiclePool** second = *s_mVehiclePool;
        if (!second) return;
        VehiclePool* pool = *second;
        if (!pool) return;
        for (auto addr : *pool) {
            if (addr) out.push_back(addr);
        }
    }
};