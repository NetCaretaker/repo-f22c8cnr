#include "aimbot.h"
#include "../../MinHook.h"



namespace game_function {
    typedef bool(__fastcall* OriginalGetIsAccurate)(__int64, __int64, Vector3, Vector3, __int64);
    OriginalGetIsAccurate originalGetIsAccurate = nullptr;
    Vector3 HitPos = Vector3(0, 0, 0);

    bool __fastcall MyGetIsAccurate(__int64 thisPtr, __int64 autiste, Vector3 vEnd, Vector3 vAdjustedEnd, __int64 lastArg) {

        
        bool silent_enabled = globals.aimbot.silent_aim;
        if (globals.aimbot.silent_hotkey.key) {
            silent_enabled = silent_enabled && ((GetAsyncKeyState(globals.aimbot.silent_hotkey.key) & 0x8000) != 0);
        }
        
        
        bool magic_enabled = globals.aimbot.magic_aim;
        if (globals.aimbot.silent_hotkey.key && magic_enabled) {
            magic_enabled = magic_enabled && ((GetAsyncKeyState(globals.aimbot.silent_hotkey.key) & 0x8000) != 0);
        }

        if ((silent_enabled || magic_enabled) && !(HitPos == Vector3(0,0,0))) {
            
            if (globals.aimbot.silent_miss_chance > 0) {
                int roll = (int)(rand() % 100);
                if (roll < globals.aimbot.silent_miss_chance) {
                    return originalGetIsAccurate ? originalGetIsAccurate(thisPtr, autiste, vEnd, vAdjustedEnd, lastArg) : false;
                }
            }
            
            
            if (magic_enabled) {
                
                Vector3 dir = HitPos - vEnd;
                float len = dir.length();
                if (len > 0.001f) {
                    dir = dir / len;
                    vEnd = HitPos + dir * 5.0f; 
                }
            }
            vAdjustedEnd = HitPos;
        }
        
        return originalGetIsAccurate ? originalGetIsAccurate(thisPtr, autiste, vEnd, vAdjustedEnd, lastArg)
                                     : false;
    }
}

bool hooked = false;

static bool IsTargetVisible(uint64_t localplayer, uint64_t targetPed) {
    (void)localplayer;

    DWORD64 camera_addr = Core::Get()->GetCamera();
    if (!camera_addr) return true;

    Vector3 cam_pos = *(Vector3*)(camera_addr + 0x60);

    Vector3 head_pos = Core::Get()->BoneVec(targetPed, 0);
    head_pos.z += 0.06f;

    // Check if target is in front of the camera (dot product with camera forward)
    DWORD64 camera_params = *(DWORD64*)(camera_addr + 0x10);
    if (camera_params) {
        Vector3 cam_fwd = *(Vector3*)(camera_params + 0x30);
        Vector3 to_target = head_pos - cam_pos;
        float dot = to_target.x * cam_fwd.x + to_target.y * cam_fwd.y + to_target.z * cam_fwd.z;
        if (dot < 0.0f) return false;
    }

    // Check if head projects on screen
    ImVec2 head_screen = Core::Get()->W2S(head_pos);
    if (head_screen.x == 0.0f && head_screen.y == 0.0f) return false;
    if (!Graphics::Get()->IsOnScreen(head_screen)) return false;

    return true;
}

void Aimbot::SetAngles(Vector3 targetpoint) {
    uint64_t pedfactory = *(uint64_t*)(Address::Get()->m_sPedFactory);
    if (!pedfactory) return;

    uint64_t localplayer = *(uint64_t*)(pedfactory + 0x8);
    if (!localplayer) return;

    DWORD64 camera_addr = Core::Get()->GetCamera();
    if (!camera_addr) return;

    Vector3 crosshair_pos = *(Vector3*)(camera_addr + 0x60);
    Vector3 direction = targetpoint - crosshair_pos;
    float distance = direction.length();
    if (distance == 0.0f) return;
    DWORD64 camera_params = *(DWORD64*)(camera_addr + 0x10);
    if (!camera_params) return;

    uint8_t ped_task = *(uint8_t*)(localplayer + Address::Get()->m_pPedTask);
    if (ped_task & (1 << 6)) {
        if (*(float*)(camera_params + 0x2AC) == -2) {
            *(float*)(camera_params + 0x2AC) = 0.0f;
            *(float*)(camera_params + 0x2C0) = 111.0f;
            *(float*)(camera_params + 0x2C4) = 111.0f;
        }
    }

    if (*(float*)(camera_params + 0x130) == 8.0f) {
        *(float*)(camera_params + 0x130) = 111.0f;
        *(float*)(camera_params + 0x134) = 111.0f;
        *(float*)(camera_params + 0x4CC) = 0.0f;
        if (*(float*)(camera_params + 0x49C) == 1.0f)
            *(float*)(camera_params + 0x49C) = 0.0f;
        *(float*)(camera_params + 0x2AC) = 0.0f;
        *(float*)(camera_params + 0x2B0) = 0.0f;
    }

    float smooth_factor = 0.5f / (globals.aimbot.smooth * 20);
    Vector3 aim_direction = direction / distance;

    if (globals.aimbot.smooth <= 1) {
        *(Vector3*)(camera_addr + 0x40) = aim_direction;
        *(Vector3*)(camera_addr + 0x3D0) = aim_direction;
    }
    else {
        auto angles_calc = [&](DWORD64 addr, const Vector3& target_direction) {
            Vector3 current_angles = *(Vector3*)(addr);
            for (int i = 0; i < 3; ++i) {
                float& current_angle = (&current_angles.x)[i];
                float target_angle = (&target_direction.x)[i];
                if (fabs(target_angle - current_angle) > smooth_factor) {
                    if (current_angle > target_angle)
                        current_angle -= smooth_factor;
                    else if (current_angle < target_angle)
                        current_angle += smooth_factor;
                }
            }
            *(Vector3*)(addr) = current_angles;
            };
        angles_calc(camera_addr + 0x40, aim_direction);
        angles_calc(camera_addr + 0x3D0, aim_direction);
    }
}

void Aimbot::SetAnglesInstant(Vector3 targetpoint) {
    DWORD64 camera_addr = Core::Get()->GetCamera();
    if (!camera_addr) return;
    Vector3 crosshair_pos = *(Vector3*)(camera_addr + 0x60);
    Vector3 direction = targetpoint - crosshair_pos;
    float distance = direction.length();
    if (distance == 0.0f) return;
    Vector3 aim_direction = direction / distance;
    *(Vector3*)(camera_addr + 0x40) = aim_direction;
    *(Vector3*)(camera_addr + 0x3D0) = aim_direction;
}
void Aimbot::Initialize()
{
    if (!hooked)
    {
        static bool mhInitialized = false;
        if (!mhInitialized) {
            if (MH_Initialize() == MH_OK || MH_Initialize() == MH_ERROR_ALREADY_INITIALIZED) {
                mhInitialized = true;
            }
        }

        if (mhInitialized) {
            
            if (MH_CreateHook((LPVOID)MemoryAddress::s_OrigAccurate,
                              (LPVOID)game_function::MyGetIsAccurate,
                              reinterpret_cast<LPVOID*>(&game_function::originalGetIsAccurate)) == MH_OK ||
                MH_CreateHook((LPVOID)MemoryAddress::s_OrigAccurate,
                              (LPVOID)game_function::MyGetIsAccurate,
                              reinterpret_cast<LPVOID*>(&game_function::originalGetIsAccurate)) == MH_ERROR_ALREADY_CREATED) {
                if (MH_EnableHook((LPVOID)MemoryAddress::s_OrigAccurate) == MH_OK ||
                    MH_EnableHook((LPVOID)MemoryAddress::s_OrigAccurate) == MH_ERROR_ENABLED) {
                    hooked = true;
                }
            }
        }
    }
    
    if (globals.aimbot.active) {
        if (GetAsyncKeyState(globals.aimbot.hotkey.key) & 0x8000) {
            uint64_t entity = GetEntity();
            if (!entity) return;
            auto bone_pos = this->GetHitbox(entity);
            ImVec2 Bone = Core::Get()->W2S(bone_pos);
            ImVec2 crosshair_pos = ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
            float dist = Core::Get()->Pythag(crosshair_pos, Bone);
            if (dist <= globals.aimbot.fov * 10) {
                this->SetAngles(bone_pos);
                if (globals.aimbot.draw_aim_line) {
                    ImVec2 crosshair_pos = ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
                    ImVec2 tgt = Core::Get()->W2S(bone_pos);
                    Graphics::Get()->DrawLine(crosshair_pos, tgt, ImColor(255,255,255,180), 1.0f);
                }
            }
        }
    }

    
    
    bool silent_enabled = globals.aimbot.silent_aim;
    if (globals.aimbot.silent_hotkey.key) {
        silent_enabled = silent_enabled && ((GetAsyncKeyState(globals.aimbot.silent_hotkey.key) & 0x8000) != 0);
    }
    
    
    bool magic_enabled = globals.aimbot.magic_aim;
    if (globals.aimbot.silent_hotkey.key && magic_enabled) {
        magic_enabled = magic_enabled && ((GetAsyncKeyState(globals.aimbot.silent_hotkey.key) & 0x8000) != 0);
    }
    
    if (silent_enabled || magic_enabled) {
        DWORD64 camera_addr = Core::Get()->GetCamera();
        if (camera_addr) {
            bool shooting = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            uint64_t entity = GetEntity();
            if (entity && shooting) {
                
                uint64_t playerinfo = *(uint64_t*)(entity + Address::Get()->m_pPlayerInfo);
                if (playerinfo) {
                    uint64_t netid = *(uint64_t*)(playerinfo + Address::Get()->m_pNetid);
                    if (g_friend_netids.find(netid) != g_friend_netids.end()) {
                        game_function::HitPos = Vector3(0, 0, 0);
                        return;
                    }
                }
                auto bone_pos = this->GetHitbox(entity);
                ImVec2 Bone = Core::Get()->W2S(bone_pos);
                ImVec2 crosshair_pos = ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
                float dist = Core::Get()->Pythag(crosshair_pos, Bone);
                float radius_px = (float)globals.aimbot.silent_fov * 10.0f;
                if (dist <= radius_px) {
                    
                    Vector3 cam = *(Vector3*)(camera_addr + 0x60);
                    Vector3 dir = (bone_pos - cam);
                    float L = dir.length();
                    if (L > 0.001f) {
                        dir = dir / L;
                        game_function::HitPos = bone_pos + dir * 3.5f; 
                    } else {
                        game_function::HitPos = bone_pos;
                    }
                } else {
                    game_function::HitPos = Vector3(0, 0, 0);
                }
            } else {
                game_function::HitPos = Vector3(0, 0, 0);
            }

            
            if (globals.aimbot.draw_aim_line) {
                if (entity) {
                    Vector3 bp = this->GetHitbox(entity);
                    ImVec2 bp2 = Core::Get()->W2S(bp);
                    ImVec2 cross2 = ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
                    float dpx = Core::Get()->Pythag(cross2, bp2);
                    float sradius = (float)globals.aimbot.silent_fov * 10.0f;
                    if (Graphics::Get()->IsOnScreen(bp2) && dpx <= sradius) {
                        Graphics::Get()->DrawLine(cross2, bp2, ImColor(255,255,255,180), 1.0f);
                    }
                }
            }
        }
    }
}
uint64_t Aimbot::GetEntity() {

    uint64_t closest_ped = 0;

    uint64_t pedfactory = *(uint64_t*)(Address::Get()->m_sPedFactory);

    uint64_t localplayer = *(uint64_t*)(pedfactory + 0x8);

    float closest_dist = FLT_MAX;
    float best_metric = FLT_MAX;
    float dist;

    for (auto cPed : **Address::Get()->s_mPedPool)
    {
        if (!cPed) {
            continue;
        }

        auto pedtype = *(uint32_t*)((uint64_t)cPed + Address::Get()->m_pEntityType);

        if (!pedtype) {
            continue;
        }
        pedtype = pedtype << 11 >> 25;

        if (!globals.aimbot.point_players) {
            if (pedtype == 2) {
                continue;
            }
        }
        if (!globals.aimbot.point_npcs) {
            if (pedtype != 2) {
                continue;
            }
        }
        if (!globals.aimbot.point_dead) {
            if (*(float*)(cPed + 0x280) <= 0) {
                continue;
            }
        }
        if (!globals.aimbot.point_animals) {
            if (pedtype > 27) {
                continue;
            }
        }

        if (cPed == localplayer)
            continue;
        
        uint64_t playerinfo = *(uint64_t*)((uint64_t)cPed + Address::Get()->m_pPlayerInfo);
        if (playerinfo) {
            uint64_t netid = *(uint64_t*)(playerinfo + Address::Get()->m_pNetid);
            if (g_friend_netids.find(netid) != g_friend_netids.end()) {
                continue;
            }
        }
        Vector3 ped_pos = *(Vector3*)(cPed + 0x90);
        Vector3 local_pos = *(Vector3*)(localplayer + 0x90);

        Vector3 dist_calc = (local_pos - ped_pos);

        double max_dist = sqrtf(dist_calc.x * dist_calc.x + dist_calc.y * dist_calc.y + dist_calc.z * dist_calc.z);

        if (max_dist <= globals.aimbot.max_dist) {
            // Visible only filter
            if (globals.aimbot.visible_only) {
                if (!IsTargetVisible(localplayer, cPed))
                    continue;
            }

            Vector3 bone_vec = Core::Get()->BoneVec(cPed, 0);
            ImVec2 head = Core::Get()->W2S(bone_vec);
            ImVec2 crosshair_pos = ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
            float dist_px = Core::Get()->Pythag(crosshair_pos, head);
            if (!Graphics::Get()->IsOnScreen(head)) continue;

            
            float metric = dist_px;
            if (globals.aimbot.target_priority == 1) { 
                metric = (float)max_dist;
            } else if (globals.aimbot.target_priority == 2) { 
                float hp = *(float*)(cPed + 0x280);
                metric = 1000.0f - hp; 
            }

            
            float radius_px = (float)globals.aimbot.silent_fov * 10.0f;
            bool in_silent = (globals.aimbot.silent_aim || globals.aimbot.magic_aim) && (dist_px <= radius_px);
            float boost = in_silent ? 0.75f : 1.0f; 
            metric *= boost;

            if (metric < best_metric) {
                best_metric = metric;
                closest_ped = cPed;
            }
        }
    }
    return closest_ped;
}
Vector3 Aimbot::GetHitbox(uint64_t entity) {
    
    if (globals.aimbot.closest_bone) {
        static const int candidateBones[] = { 0, 7, 8, 5, 6, 2, 1 }; 
        ImVec2 cross = ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
        float best = FLT_MAX;
        Vector3 bestPos = Core::Get()->BoneVec(entity, 0);
        for (int i = 0; i < (int)(sizeof(candidateBones)/sizeof(candidateBones[0])); ++i) {
            Vector3 p = Core::Get()->BoneVec(entity, candidateBones[i]);
            if (candidateBones[i] == 0) p.z += 0.06f;
            ImVec2 s = Core::Get()->W2S(p);
            if (!Graphics::Get()->IsOnScreen(s)) continue;
            float d = Core::Get()->Pythag(cross, s);
            if (d < best) { best = d; bestPos = p; }
        }
        return bestPos;
    }

    Vector3 bone_pos;
    switch (globals.aimbot.bone) {
    case 0: bone_pos = Core::Get()->BoneVec(entity, 0); bone_pos.z += 0.06f; break;
    case 1: bone_pos = Core::Get()->BoneVec(entity, 7); break;
    case 2: bone_pos = Core::Get()->BoneVec(entity, 8); break;
    case 3: bone_pos = Core::Get()->BoneVec(entity, 5); break;
    case 4: bone_pos = Core::Get()->BoneVec(entity, 6); break;
    case 5: bone_pos = Core::Get()->BoneVec(entity, 2); break;
    case 6: bone_pos = Core::Get()->BoneVec(entity, 1); break;
    }
    return bone_pos;
}