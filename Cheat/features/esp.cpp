#include "esp.h"
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
inline std::unordered_map<uintptr_t, std::unordered_map<unsigned int, int>> StoredBonesIdx;
enum BoneMasks : int { SKEL_ROOT = 0x0, SKEL_Pelvis = 0x2e28, SKEL_L_Thigh = 0xe39f, SKEL_L_Calf = 0xf9bb, SKEL_L_Foot = 0x3779, SKEL_L_Toe0 = 0x83c, IK_L_Foot = 0xfedd, PH_L_Foot = 0xe175, MH_L_Knee = 0xb3fe, SKEL_R_Thigh = 0xca72, SKEL_R_Calf = 0x9000, SKEL_R_Foot = 0xcc4d, SKEL_R_Toe0 = 0x512d, IK_R_Foot = 0x8aae, PH_R_Foot = 0x60e6, MH_R_Knee = 0x3fcf, RB_L_ThighRoll = 0x5c57, RB_R_ThighRoll = 0x192a, SKEL_Spine_Root = 0xe0fd, SKEL_Spine0 = 0x5c01, SKEL_Spine1 = 0x60f0, SKEL_Spine2 = 0x60f1, SKEL_Spine3 = 0x60f2, SKEL_L_Clavicle = 0xfcd9, SKEL_L_UpperArm = 0xb1c5, SKEL_L_Forearm = 0xeeeb, SKEL_L_Hand = 0x49d9, SKEL_L_Finger00 = 0x67f2, SKEL_L_Finger01 = 0xff9, SKEL_L_Finger02 = 0xffa, SKEL_L_Finger10 = 0x67f3, SKEL_L_Finger11 = 0x1049, SKEL_L_Finger12 = 0x104a, SKEL_L_Finger20 = 0x67f4, SKEL_L_Finger21 = 0x1059, SKEL_L_Finger22 = 0x105a, SKEL_L_Finger30 = 0x67f5, SKEL_L_Finger31 = 0x1029, SKEL_L_Finger32 = 0x102a, SKEL_L_Finger40 = 0x67f6, SKEL_L_Finger41 = 0x1039, SKEL_L_Finger42 = 0x103a, PH_L_Hand = 0xeb95, IK_L_Hand = 0x8cbd, RB_L_ForeArmRoll = 0xee4f, RB_L_ArmRoll = 0x1470, MH_L_Elbow = 0x58b7, SKEL_R_Clavicle = 0x29d2, SKEL_R_UpperArm = 0x9d4d, SKEL_R_Forearm = 0x6e5c, SKEL_R_Hand = 0xdead, SKEL_R_Finger00 = 0xe5f2, SKEL_R_Finger01 = 0xfa10, SKEL_R_Finger02 = 0xfa11, SKEL_R_Finger10 = 0xe5f3, SKEL_R_Finger11 = 0xfa60, SKEL_R_Finger12 = 0xfa61, SKEL_R_Finger20 = 0xe5f4, SKEL_R_Finger21 = 0xfa70, SKEL_R_Finger22 = 0xfa71, SKEL_R_Finger30 = 0xe5f5, SKEL_R_Finger31 = 0xfa40, SKEL_R_Finger32 = 0xfa41, SKEL_R_Finger40 = 0xe5f6, SKEL_R_Finger41 = 0xfa50, SKEL_R_Finger42 = 0xfa51, PH_R_Hand = 0x6f06, IK_R_Hand = 0x188e, RB_R_ForeArmRoll = 0xab22, RB_R_ArmRoll = 0x90ff, MH_R_Elbow = 0xbb0, SKEL_Neck_1 = 0x9995, SKEL_Head = 0x796e, IK_Head = 0x322c, FACIAL_facialRoot = 0xfe2c, FB_L_Brow_Out_000 = 0xe3db, FB_L_Lid_Upper_000 = 0xb2b6, FB_L_Eye_000 = 0x62ac, FB_L_CheekBone_000 = 0x542e, FB_L_Lip_Corner_000 = 0x74ac, FB_R_Lid_Upper_000 = 0xaa10, FB_R_Eye_000 = 0x6b52, FB_R_CheekBone_000 = 0x4b88, FB_R_Brow_Out_000 = 0x54c, FB_R_Lip_Corner_000 = 0x2ba6, FB_Brow_Centre_000 = 0x9149, FB_UpperLipRoot_000 = 0x4ed2, FB_UpperLip_000 = 0xf18f, FB_L_Lip_Top_000 = 0x4f37, FB_R_Lip_Top_000 = 0x4537, FB_Jaw_000 = 0xb4a0, FB_LowerLipRoot_000 = 0x4324, FB_LowerLip_000 = 0x508f, FB_L_Lip_Bot_000 = 0xb93b, FB_R_Lip_Bot_000 = 0xc33b, FB_Tongue_000 = 0xb987, RB_Neck_1 = 0x8b93, IK_Root = 0xdd1c };

void getSkeleShit(uintptr_t& fragInst, uintptr_t& v9, uintptr_t pedaddy) {
    __try
    {
        fragInst = *(uintptr_t*)((uintptr_t)pedaddy + 0x1430);
        v9 = *(uintptr_t*)(fragInst + 0x68);
    }
    __except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { return; }

}
struct cSkeleton_t {

    std::unordered_map<unsigned int, int> StoredBonesIdx;

    struct crSkeletonData_t {
        uintptr_t Ptr, m_BoneIdTable;
        unsigned int m_Used; 
        unsigned int m_NumBones; 
        unsigned short m_BoneIdTable_Slots;
    } crSkeletonData;

    uintptr_t m_pSkeleton, Arg2;
    XMMATRIX Arg1;
};
inline XMFLOAT3 GetBonePosInstFrag(std::uintptr_t InstFrag, unsigned int BoneID, XMMATRIX Arg1, uintptr_t Arg2)
{
    if (!Arg1.r[0].m128_f32[0]) 
        return XMFLOAT3(0, 0, 0);

    XMMATRIX Result = *(XMMATRIX*)(Arg2 + (static_cast<uint64_t>(BoneID) << 6));
    if (!Result.r[0].m128_f32[0])
        return XMFLOAT3(0, 0, 0);

    XMFLOAT3 vec1(Arg1.r[0].m128_f32[0], Arg1.r[0].m128_f32[1], Arg1.r[0].m128_f32[2]);
    XMFLOAT3 vec2(Arg1.r[1].m128_f32[0], Arg1.r[1].m128_f32[1], Arg1.r[1].m128_f32[2]);
    XMFLOAT3 vec3(Arg1.r[2].m128_f32[0], Arg1.r[2].m128_f32[1], Arg1.r[2].m128_f32[2]);
    XMFLOAT3 vec4(Arg1.r[3].m128_f32[0], Arg1.r[3].m128_f32[1], Arg1.r[3].m128_f32[2]);
    XMFLOAT3 vec5(Result.r[3].m128_f32[0], Result.r[3].m128_f32[1], Result.r[3].m128_f32[2]);

    return XMFLOAT3(
        vec1.x * vec5.x + vec4.x + vec2.x * vec5.y + vec3.x * vec5.z,
        vec1.y * vec5.x + vec4.y + vec2.y * vec5.y + vec3.y * vec5.z,
        vec1.z * vec5.x + vec4.z + vec2.z * vec5.y + vec3.z * vec5.z
    );
}
inline bool GetBoneIndex(cSkeleton_t cSkeleton, unsigned int boneId, int& outBoneIdx)
{
    auto& StoredBonesIdxCache = StoredBonesIdx[cSkeleton.crSkeletonData.Ptr];

    if (StoredBonesIdxCache.find(boneId) != StoredBonesIdxCache.end()) {
        outBoneIdx = StoredBonesIdxCache[boneId];
        return true;
    }

    if (cSkeleton.crSkeletonData.m_Used != 0) 
    {
        unsigned short m_BoneIdTable_Slots = cSkeleton.crSkeletonData.m_BoneIdTable_Slots; 

        std::uintptr_t m_BoneIdTable_Hash = *(std::uintptr_t*)(cSkeleton.crSkeletonData.m_BoneIdTable + 0x8 * (boneId % (unsigned int)m_BoneIdTable_Slots));
        for (std::uintptr_t i = m_BoneIdTable_Hash; i != 0; i = *(std::uintptr_t*)(i + 0x8))
        {
            int i_key = *(int*)(i);
            if (boneId == i_key)
            {
                int p_Data = *(int*)(i + 0x4);
                if (p_Data)
                {
                    outBoneIdx = p_Data;
                    StoredBonesIdxCache[boneId] = p_Data;
                    return true;
                }
            }
        }
    }
    else if (boneId < cSkeleton.crSkeletonData.m_NumBones)
    {
        outBoneIdx = boneId;
        StoredBonesIdxCache[boneId] = boneId;

        return true;
    }

    return false;
}
XMFLOAT2 WorldToScreenXM(XMFLOAT3 vPos)
{

    XMMATRIX viewmatrix = XMMatrixTranspose(XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(Address::Get()->s_pViewPort + 0x24C)));
    XMFLOAT4 vec_x, vec_y, vec_z;

    XMStoreFloat4(&vec_x, viewmatrix.r[1]);
    XMStoreFloat4(&vec_y, viewmatrix.r[2]);
    XMStoreFloat4(&vec_z, viewmatrix.r[3]);

    XMFLOAT3 screen_pos;
    XMVECTOR vPosVec = XMLoadFloat3(&vPos);

    screen_pos.x = vec_x.x * vPos.x + vec_x.y * vPos.y + vec_x.z * vPos.z + vec_x.w;
    screen_pos.y = vec_y.x * vPos.x + vec_y.y * vPos.y + vec_y.z * vPos.z + vec_y.w;
    screen_pos.z = vec_z.x * vPos.x + vec_z.y * vPos.y + vec_z.z * vPos.z + vec_z.w;

    if (screen_pos.z <= 0.1f) return XMFLOAT2(0, 0);

    screen_pos.z = 1.0f / screen_pos.z;
    screen_pos.x *= screen_pos.z;
    screen_pos.y *= screen_pos.z;

    auto width = GetSystemMetrics(SM_CXSCREEN);
    auto height = GetSystemMetrics(SM_CYSCREEN);

    float x_temp = width / 2.0f;
    float y_temp = height / 2.0f;

    screen_pos.x = x_temp + (screen_pos.x * x_temp);
    screen_pos.y = y_temp - (screen_pos.y * y_temp);

    return XMFLOAT2(screen_pos.x, screen_pos.y);
}
inline XMFLOAT3 GetBonePosComplex(XMFLOAT3 Ped, unsigned int Mask, cSkeleton_t cSkeleton)
{
    int BoneId = 0;

    if (GetBoneIndex(cSkeleton, Mask, BoneId))
    {
        return GetBonePosInstFrag(cSkeleton.m_pSkeleton, BoneId, cSkeleton.Arg1, cSkeleton.Arg2);
    }

    return Ped;
}

void Visuals::Initialize()
{
    if (GetAsyncKeyState(globals.visuals.master_switch.key) & 1) {

        globals.visuals.activate = !globals.visuals.activate;
    }
    if (globals.aimbot.draw_fov) {

        ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2), globals.aimbot.fov * 10, ImColor{ globals.visuals.fov_color[0], globals.visuals.fov_color[1], globals.visuals.fov_color[2], globals.visuals.fov_color[3] }, 100.0f);
        ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2), globals.aimbot.silent_fov * 10, ImColor{ globals.visuals.fov_color[0], globals.visuals.fov_color[1], globals.visuals.fov_color[2], globals.visuals.fov_color[3] }, 100.0f);

    }

    if (globals.visuals.activate) {

        uint64_t pedfactory = *(uint64_t*)(Address::Get()->m_sPedFactory);

        if (!pedfactory) return;

        uint64_t localplayer = *(uint64_t*)(pedfactory + 0x8);

        if (!localplayer) return;

        int drawn_players = 0;
        int drawn_npcs = 0;
        int drawn_skeletons = 0;

        if (!Address::Get()->s_mPedPool || !*Address::Get()->s_mPedPool) return;

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

            
            bool is_player = (pedtype == 2);
            if (is_player) {
                if (!globals.visuals.show_npcs) {
                    
                }
            } else {
                if (!globals.visuals.show_npcs) {
                    continue;
                }
            }

            if (!globals.visuals.show_dead) {
                if (*(float*)(cPed + 0x280) <= 0) {
                    continue;
                }
            }
            if (!globals.visuals.show_self) {
                if (cPed == localplayer) {
                    continue;
                }
            }
            if (!globals.visuals.show_animals) {
                if (pedtype > 27) {
                    continue;
                }
            }

            
            if (is_player) {
                if (drawn_players >= globals.visuals.player_max_display) continue;
            } else {
                if (drawn_npcs >= globals.visuals.npc_max_display) continue;
            }

            
            bool is_friend = false;
            {
                uint64_t playerinfo = *(uint64_t*)((uint64_t)cPed + Address::Get()->m_pPlayerInfo);
                if (playerinfo) {
                    uint64_t netid = *(uint64_t*)(playerinfo + Address::Get()->m_pNetid);
                    is_friend = (g_friend_netids.find(netid) != g_friend_netids.end());
                }
            }

            Vector3 ped_pos_vector = *(Vector3*)(cPed + 0x90);
            Vector3 local_pos_Vector3 = *(Vector3*)(localplayer + 0x90);
            ImVec2 ped_visual_pos = Core::Get()->W2S(ped_pos_vector);

            ImVec2 head = Core::Get()->W2S(Vector3(ped_pos_vector.x, ped_pos_vector.y, ped_pos_vector.z + 0.8f));

            ImVec2 head1 = Core::Get()->W2S(Vector3(ped_pos_vector.x, ped_pos_vector.y, ped_pos_vector.z + 0.9f));

            ImVec2 head2 = Core::Get()->W2S(Vector3(ped_pos_vector.x, ped_pos_vector.y, ped_pos_vector.z + 0.91f));

            ImVec2 foot = Core::Get()->W2S(Vector3(ped_pos_vector.x, ped_pos_vector.y, ped_pos_vector.z - 0.95f));

            ImVec2 foot1 = Core::Get()->W2S(Vector3(ped_pos_vector.x, ped_pos_vector.y, ped_pos_vector.z - 0.96f));

            ImVec2 foot2 = Core::Get()->W2S(Vector3(ped_pos_vector.x, ped_pos_vector.y, ped_pos_vector.z - 0.97f));

            float h = head.y - foot.y;
            float w = (foot.y - head.y) / 4.5f;
            float health_calculation = *(float*)(cPed + 0x280) - 100;
            float armor_calculation = *(float*)(cPed + Address::Get()->m_pArmor);
            Vector3 distance_calculation = (local_pos_Vector3 - ped_pos_vector);
            double view_distance = sqrtf(distance_calculation.x * distance_calculation.x + distance_calculation.y * distance_calculation.y + distance_calculation.z * distance_calculation.z);
            float height = foot.y - head.y;

            if (!Graphics::Get()->IsOnScreen(ped_visual_pos)) continue;
            if (!Graphics::Get()->IsOnScreen(head)) continue;
            if (!Graphics::Get()->IsOnScreen(head1)) continue;
            if (!Graphics::Get()->IsOnScreen(head2)) continue;
            if (!Graphics::Get()->IsOnScreen(foot)) continue;
            if (!Graphics::Get()->IsOnScreen(foot1)) continue;
            if (!Graphics::Get()->IsOnScreen(foot2)) continue;

            if (view_distance < globals.visuals.view_distance) {

                if (globals.visuals.box) {

                    if (Graphics::Get()->IsOnScreen({ foot.x - w, foot.y }) && Graphics::Get()->IsOnScreen({ foot.x + w, head.y })) {
                        ImVec4 baseBoxCol(globals.visuals.boxcolor[0], globals.visuals.boxcolor[1], globals.visuals.boxcolor[2], globals.visuals.boxcolor[3]);
                        if (is_friend) baseBoxCol = ImVec4(0.2f, 0.95f, 0.2f, baseBoxCol.w);
                        ImU32 boxCol = ImGui::GetColorU32(baseBoxCol);
                        if (globals.visuals.boxstyle == 0) {
                            ImGui::GetBackgroundDrawList()->AddRect({ foot.x - w, foot.y }, { foot.x + w, head.y }, boxCol, 0, 0, globals.visuals.box_thickness);
                        }
                        else if (globals.visuals.boxstyle == 1) {

                            ImGui::GetBackgroundDrawList()->AddRect({ foot.x - w, foot.y }, { foot.x + w, head.y }, boxCol, 0, 0, globals.visuals.box_thickness);
                            ImGui::GetBackgroundDrawList()->AddRectFilled({ foot.x - w, foot.y }, { foot.x + w, head.y }, ImColor(0, 0, 0, 70));
                        }
                        else if (globals.visuals.boxstyle == 2) {
                            float w2 = (foot.y - head.y) * 0.495f;
                            float h2 = foot.y - head.y;
                            float drawpos = (((foot.y - head.y) * 0.495f) / 2);
                            Graphics::Get()->DrawCornerBox(foot.x - drawpos, head.y, w2, h2, globals.visuals.box_thickness, 0.25f, boxCol);
                        }
                    }
                }
                if (globals.visuals.skeleton) {
                    
                    if ((int)view_distance <= globals.visuals.skeleton_max_distance && drawn_skeletons < globals.visuals.skeleton_max_display) {
                        uintptr_t fragInst = NULL;
                        uintptr_t v9 = NULL;
                        getSkeleShit(fragInst, v9, cPed);

                        if (!v9)
                            goto __skip_skeleton;
                        cSkeleton_t Skeleton;
                        Skeleton.m_pSkeleton = *(uintptr_t*)(v9 + 0x178);

                        Skeleton.crSkeletonData.Ptr = *(uintptr_t*)(Skeleton.m_pSkeleton);
                        Skeleton.crSkeletonData.m_Used = *(unsigned int*)(Skeleton.crSkeletonData.Ptr + 0x1A);
                        Skeleton.crSkeletonData.m_NumBones = *(unsigned int*)(Skeleton.crSkeletonData.Ptr + 0x5E);
                        Skeleton.crSkeletonData.m_BoneIdTable_Slots = *(unsigned short*)(Skeleton.crSkeletonData.Ptr + 0x18);

                        if (!Skeleton.crSkeletonData.m_BoneIdTable_Slots)
                            goto __skip_skeleton;

                        Skeleton.crSkeletonData.m_BoneIdTable = *(uintptr_t*)(Skeleton.crSkeletonData.Ptr + 0x10);

                        Skeleton.Arg1 = *(XMMATRIX*)(*(uintptr_t*)(Skeleton.m_pSkeleton + 0x8));
                        Skeleton.Arg2 = *(uintptr_t*)(Skeleton.m_pSkeleton + 0x18);

                        XMFLOAT3 PelvisPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_Pelvis, Skeleton);
                        XMFLOAT3 NeckPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_Neck_1, Skeleton);
                        
                        NeckPos.z += 0.06f;
                        XMFLOAT3 LeftUperarmPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_L_UpperArm, Skeleton);
                        XMFLOAT3 RightUperarmPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_R_UpperArm, Skeleton);
                        XMFLOAT3 RightFormArmPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_R_Forearm, Skeleton);
                        XMFLOAT3 LeftFormArmPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_L_Forearm, Skeleton);
                        XMFLOAT3 RightHandPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_R_Hand, Skeleton);
                        XMFLOAT3 LeftHandPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_L_Hand, Skeleton);
                        XMFLOAT3 LeftClaviclePos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_L_Clavicle, Skeleton);
                        XMFLOAT3 RightClaviclePos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_R_Clavicle, Skeleton);
                        XMFLOAT3 HeadPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_Head, Skeleton);
                        XMFLOAT3 LeftThighPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_L_Thigh, Skeleton);
                        XMFLOAT3 LeftCalfPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_L_Calf, Skeleton);
                        XMFLOAT3 RightThighPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_R_Thigh, Skeleton);
                        XMFLOAT3 RightCalfPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_R_Calf, Skeleton);
                        XMFLOAT3 LfootPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_L_Foot, Skeleton);
                        XMFLOAT3 RfootPos = GetBonePosComplex(XMFLOAT3(0, 0, 0), SKEL_R_Foot, Skeleton);

                        ImVec2 Pelvis = Core::Get()->W2S(Vector3(PelvisPos.x, PelvisPos.y, PelvisPos.z));
                        ImVec2 Neck = Core::Get()->W2S(Vector3(NeckPos.x, NeckPos.y, NeckPos.z));
                        ImVec2 LeftUperarm = Core::Get()->W2S(Vector3(LeftUperarmPos.x, LeftUperarmPos.y, LeftUperarmPos.z));
                        ImVec2 RightUperarm = Core::Get()->W2S(Vector3(RightUperarmPos.x, RightUperarmPos.y, RightUperarmPos.z));
                        ImVec2 RightFormArm = Core::Get()->W2S(Vector3(RightFormArmPos.x, RightFormArmPos.y, RightFormArmPos.z));
                        ImVec2 LeftFormArm = Core::Get()->W2S(Vector3(LeftFormArmPos.x, LeftFormArmPos.y, LeftFormArmPos.z));
                        ImVec2 RightHand = Core::Get()->W2S(Vector3(RightHandPos.x, RightHandPos.y, RightHandPos.z));
                        ImVec2 LeftHand = Core::Get()->W2S(Vector3(LeftHandPos.x, LeftHandPos.y, LeftHandPos.z));
                        ImVec2 LeftClavicle = Core::Get()->W2S(Vector3(LeftClaviclePos.x, LeftClaviclePos.y, LeftClaviclePos.z));
                        ImVec2 RightClavicle = Core::Get()->W2S(Vector3(RightClaviclePos.x, RightClaviclePos.y, RightClaviclePos.z));
                        ImVec2 HeadScreen = Core::Get()->W2S(Vector3(HeadPos.x, HeadPos.y, HeadPos.z));
                        ImVec2 LeftThigh = Core::Get()->W2S(Vector3(LeftThighPos.x, LeftThighPos.y, LeftThighPos.z));
                        ImVec2 LeftCalf = Core::Get()->W2S(Vector3(LeftCalfPos.x, LeftCalfPos.y, LeftCalfPos.z));
                        ImVec2 RightThigh = Core::Get()->W2S(Vector3(RightThighPos.x, RightThighPos.y, RightThighPos.z));
                        ImVec2 RightCalf = Core::Get()->W2S(Vector3(RightCalfPos.x, RightCalfPos.y, RightCalfPos.z));
                        ImVec2 Lfoot = Core::Get()->W2S(Vector3(LfootPos.x, LfootPos.y, LfootPos.z));
                        ImVec2 Rfoot = Core::Get()->W2S(Vector3(RfootPos.x, RfootPos.y, RfootPos.z));
                        ImDrawList* DrawList = ImGui::GetBackgroundDrawList();
                        float skelThickness = (float)globals.visuals.skeleton_thickness;
                        ImVec4 skelC(globals.visuals.skeleton_color[0], globals.visuals.skeleton_color[1], globals.visuals.skeleton_color[2], globals.visuals.skeleton_color[3]);
                        if (is_friend) skelC = ImVec4(0.2f, 0.95f, 0.2f, skelC.w);
                        ImU32 skelColor = ImGui::GetColorU32(skelC);


                        if (globals.visuals.skeleton_type == 0) {
                            DrawList->AddLine(ImVec2(LeftUperarm.x, LeftUperarm.y), ImVec2(RightUperarm.x, RightUperarm.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(RightUperarm.x, RightUperarm.y), ImVec2(RightFormArm.x, RightFormArm.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(LeftUperarm.x, LeftUperarm.y), ImVec2(LeftFormArm.x, LeftFormArm.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(RightFormArm.x, RightFormArm.y), ImVec2(RightHand.x, RightHand.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(LeftFormArm.x, LeftFormArm.y), ImVec2(LeftHand.x, LeftHand.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(Neck.x, Neck.y), ImVec2(Pelvis.x, Pelvis.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(Pelvis.x, Pelvis.y), ImVec2(LeftThigh.x, LeftThigh.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(Pelvis.x, Pelvis.y), ImVec2(RightThigh.x, RightThigh.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(LeftThigh.x, LeftThigh.y), ImVec2(LeftCalf.x, LeftCalf.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(RightThigh.x, RightThigh.y), ImVec2(RightCalf.x, RightCalf.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(LeftCalf.x, LeftCalf.y), ImVec2(Lfoot.x, Lfoot.y), skelColor, skelThickness);
                            DrawList->AddLine(ImVec2(RightCalf.x, RightCalf.y), ImVec2(Rfoot.x, Rfoot.y), skelColor, skelThickness);
                        } else {
                            ImU32 outlineColor = ImGui::GetColorU32(ImVec4(0.f, 0.f, 0.f, skelC.w));
                            float outlineThickness = skelThickness + 2.0f;
                            if (outlineThickness < skelThickness + 1.0f) outlineThickness = skelThickness + 1.0f;
                            float height2D = fabsf(foot.y - head.y);
                            float jointRadius = height2D * 0.008f;
                            if (jointRadius < 2.0f) jointRadius = 2.0f;
                            if (jointRadius > 5.0f) jointRadius = 5.0f;

                            auto drawBone = [&](const ImVec2& a, const ImVec2& b) {
                                DrawList->AddLine(a, b, outlineColor, outlineThickness);
                                DrawList->AddLine(a, b, skelColor, skelThickness);
                            };
                            auto drawJoint = [&](const ImVec2& p) {
                                DrawList->AddCircle(p, jointRadius + 1.0f, outlineColor, 12, 2.0f);
                                DrawList->AddCircleFilled(p, jointRadius, skelColor, 12);
                            };

                            
                            drawBone(Neck, Pelvis);
                            drawBone(Neck, HeadScreen);
                            DrawList->AddCircle(HeadScreen, jointRadius * 1.5f + 1.0f, outlineColor, 20, 2.0f);
                            DrawList->AddCircle(HeadScreen, jointRadius * 1.5f, skelColor, 20, skelThickness);

                            
                            drawBone(Neck, LeftClavicle);
                            drawBone(LeftClavicle, LeftUperarm);
                            drawBone(LeftUperarm, LeftFormArm);
                            drawBone(LeftFormArm, LeftHand);

                            
                            drawBone(Neck, RightClavicle);
                            drawBone(RightClavicle, RightUperarm);
                            drawBone(RightUperarm, RightFormArm);
                            drawBone(RightFormArm, RightHand);

                            
                            drawBone(Pelvis, LeftThigh);
                            drawBone(LeftThigh, LeftCalf);
                            drawBone(LeftCalf, Lfoot);
                            drawBone(Pelvis, RightThigh);
                            drawBone(RightThigh, RightCalf);
                            drawBone(RightCalf, Rfoot);

                            
                            drawJoint(Neck);
                            drawJoint(Pelvis);
                            drawJoint(LeftClavicle);
                            drawJoint(RightClavicle);
                            drawJoint(LeftUperarm);
                            drawJoint(RightUperarm);
                            drawJoint(LeftFormArm);
                            drawJoint(RightFormArm);
                            drawJoint(LeftHand);
                            drawJoint(RightHand);
                            drawJoint(LeftThigh);
                            drawJoint(RightThigh);
                            drawJoint(LeftCalf);
                            drawJoint(RightCalf);
                            drawJoint(Lfoot);
                            drawJoint(Rfoot);
                        }

                        drawn_skeletons++;
                    }
                }
__skip_skeleton:
                if (globals.visuals.names) {

                    std::string player_name_string_esp = "Player"; 
                    uint64_t playerinfo = *(uint64_t*)(cPed + Address::Get()->m_pPlayerInfo);
                    if (playerinfo) {
                        int netid = *(uint64_t*)(playerinfo + Address::Get()->m_pNetid);
                        std::string name = Address::Get()->GetPlayerNameByNetId(netid);
                        if (name != "NPC" && !name.empty()) {
                            player_name_string_esp = name;
                        }
                        else if (netid > 0) {
                            player_name_string_esp = "Player " + std::to_string(netid);
                        }
                    }
                    ImVec2 screenNamePos(head1.x, head1.y - 15);
                    ImVec4 nameC(globals.visuals.namecolor[0], globals.visuals.namecolor[1], globals.visuals.namecolor[2], globals.visuals.namecolor[3]);
                    if (is_friend) nameC = ImVec4(0.2f, 0.95f, 0.2f, nameC.w);
                    Graphics::Get()->DrawTextA(ImGui::GetFont(), player_name_string_esp, ImVec2(head.x, head.y - 10), globals.visuals.text_size, ImGui::GetColorU32(nameC), true);
                }
                if (globals.visuals.snapline)
                {
                    ImVec2 localposw2s = Core::Get()->W2S(local_pos_Vector3);
                    ImVec4 lineC(globals.visuals.snapline_color[0], globals.visuals.snapline_color[1], globals.visuals.snapline_color[2], globals.visuals.snapline_color[3]);
                    if (is_friend) lineC = ImVec4(0.2f, 0.95f, 0.2f, lineC.w);
                    Graphics::Get()->DrawLine(localposw2s, ped_visual_pos, ImGui::GetColorU32(lineC), globals.visuals.snapline_thickness);
                }

                if (globals.visuals.healthbar) {
                    ImColor healthcolor;
                    if (health_calculation > 75) {
                        healthcolor = ImColor(0, 255, 0); 
                    }
                    else if (health_calculation > 50) {
                        healthcolor = ImColor(255, 255, 0); 
                    }
                    else if (health_calculation > 25) {
                        healthcolor = ImColor(255, 165, 0); 
                    }
                    else if (health_calculation > 15) {
                        healthcolor = ImColor(255, 69, 0); 
                    }
                    else {
                        healthcolor = ImColor(255, 0, 0); 
                    }

                    float healthRatio = health_calculation / 100.0f;

                    if (globals.visuals.healthposition == 0) {
                        float healthHeight = h * healthRatio;
                        Graphics::Get()->DrawBorder(foot.x - w - 7, foot.y + 1, 3, h - 2, 1, ImColor(0, 0, 0));
                        Graphics::Get()->FillRect(foot.x - w - 6, foot.y, 2, h, ImColor(0, 0, 0));
                        Graphics::Get()->FillRect(foot.x - w - 6, foot.y, 2, healthHeight, healthcolor);
                    }
                    else if (globals.visuals.healthposition == 1) {
                        float cur_w = w * 2;
                        float healthWidth = cur_w * healthRatio;
                        if (!globals.visuals.box) {
                            Graphics::Get()->DrawBorder(foot.x - w - 1, foot.y - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot.x - w, foot.y, cur_w, 2.5f, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot.x - w, foot.y, healthWidth, 2.5f, healthcolor);
                        }
                        else {
                            Graphics::Get()->DrawBorder(foot1.x - w - 1, foot1.y + 3 - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot1.x - w, foot1.y + 3, cur_w, 2.5f, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot1.x - w, foot1.y + 3, healthWidth, 2.5f, healthcolor);
                        }
                    }
                    else if (globals.visuals.healthposition == 2) {
                        float cur_w = w * 2;
                        float healthWidth = cur_w * healthRatio;
                        if (globals.visuals.armorposition == 2) {
                            Graphics::Get()->DrawBorder(foot2.x - w - 1, head2.y + 8 - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot2.x - w, head2.y + 8, cur_w, 2.5f, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot2.x - w, head2.y + 8, healthWidth, 2.5f, healthcolor);
                        }
                        else {
                            Graphics::Get()->DrawBorder(foot1.x - w - 1, head1.y + 3 - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot1.x - w, head1.y + 3, cur_w, 2.5f, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot1.x - w, head1.y + 3, healthWidth, 2.5f, healthcolor);
                        }
                    }
                }

                if (globals.visuals.armorbar) {
                    ImColor armorColor = ImColor(0, 0, 255); 
                    float armorRatio = armor_calculation / 100.0f;

                    if (globals.visuals.armorposition == 0) {
                        if (armor_calculation > 0) {
                            float step = (h / 100);
                            float draw = (step * armor_calculation);
                            Graphics::Get()->DrawBorder(foot.x - w - 12, foot.y + 1, 3, h - 2, 1, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot.x - w - 11, foot.y, 2, h, ImColor(0, 0, 0));
                            Graphics::Get()->FillRect(foot.x - w - 11, foot.y, 2, draw, ImColor(58, 95, 205));
                        }
                    }
                    else if (globals.visuals.armorposition == 1) {
                        if (globals.visuals.armorposition == 1) {
                            if (!globals.visuals.box) {
                                if (armor_calculation > 0) {
                                    float cur_w = w + w;
                                    float W_HEALTH = (cur_w / 100) * armor_calculation;
                                    Graphics::Get()->DrawBorder(foot1.x - w - 1, foot1.y + 5 - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                                    Graphics::Get()->FillRect(foot1.x - w, foot1.y + 5, cur_w, 2.5f, ImColor(0, 0, 0), 0);
                                    Graphics::Get()->FillRect(foot1.x - w, foot1.y + 5, W_HEALTH, 2.5f, ImColor(58, 95, 205), 0);
                                }
                            }
                            else {
                                if (armor_calculation > 0) {
                                    float cur_w = w + w;
                                    float W_HEALTH = (cur_w / 100) * armor_calculation;
                                    Graphics::Get()->DrawBorder(foot2.x - w - 1, foot2.y + 8 - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                                    Graphics::Get()->FillRect(foot2.x - w, foot2.y + 8, cur_w, 2.5f, ImColor(0, 0, 0), 0);
                                    Graphics::Get()->FillRect(foot2.x - w, foot2.y + 8, W_HEALTH, 2.5f, ImColor(58, 95, 205), 0);

                                }
                            }

                        }
                        else {
                            if (!globals.visuals.box) {
                                if (armor_calculation > 0) {
                                    float cur_w = w + w;
                                    float W_HEALTH = (cur_w / 100) * armor_calculation;
                                    Graphics::Get()->DrawBorder(foot.x - w - 1, foot.y - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                                    Graphics::Get()->FillRect(foot.x - w, foot.y, cur_w, 2.5f, ImColor(0, 0, 0), 0);
                                    Graphics::Get()->FillRect(foot.x - w, foot.y, W_HEALTH, 2.5f, ImColor(58, 95, 205), 0);
                                }
                            }
                            else {
                                float cur_w = w + w;
                                float W_HEALTH = (cur_w / 100) * armor_calculation;
                                Graphics::Get()->DrawBorder(foot1.x - w - 1, foot1.y + 3 - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                                Graphics::Get()->FillRect(foot1.x - w, foot1.y + 3, cur_w, 2.5f, ImColor(0, 0, 0), 0);
                                Graphics::Get()->FillRect(foot1.x - w, foot1.y + 3, W_HEALTH, 2.5f, ImColor(58, 95, 205), 0);
                            }

                        }

                    }
                    else if (globals.visuals.armorposition == 2) {

                        float cur_w = w + w;
                        float W_HEALTH = (cur_w / 100) * armor_calculation;
                        Graphics::Get()->DrawBorder(foot1.x - w - 1, head1.y + 3 - 1, cur_w + 1, 2.5f + 1, 1, ImColor(0, 0, 0));
                        Graphics::Get()->FillRect(foot1.x - w, head1.y + 3, cur_w, 2.5f, ImColor(0, 0, 0), 0);
                        Graphics::Get()->FillRect(foot1.x - w, head1.y + 3, W_HEALTH, 2.5f, ImColor(58, 95, 205), 0);
                    }
                }
                if (globals.visuals.distance) {
                    int renderdistance1 = sqrtf(distance_calculation.x * distance_calculation.x + distance_calculation.y * distance_calculation.y + distance_calculation.z * distance_calculation.z);
                    std::string distancestring = std::to_string(renderdistance1) + "m";
                    if (!globals.visuals.weapon_name) {
                        if (globals.visuals.healthposition == 1) {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y + 3), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                        else if (globals.visuals.armorposition == 1) {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y + 3), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                        else if (globals.visuals.healthposition == 1 && globals.visuals.armorposition == 1) {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y + 10), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                        else if (globals.visuals.healthposition == 0 && globals.visuals.armorposition == 0) {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                        else {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                    }
                    else {
                        if (globals.visuals.healthposition == 1 || globals.visuals.armorposition == 1) {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y + 12), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                        else if (globals.visuals.healthposition == 1 && globals.visuals.armorposition == 1) {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y + 20), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                        else if (globals.visuals.healthposition == 0 && globals.visuals.armorposition == 0) {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y + 6), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                        else {
                            Graphics::Get()->DrawTextA(ImGui::GetFont(), distancestring, ImVec2(foot.x, foot.y + 6), globals.visuals.text_size, ImColor{ globals.visuals.distance_color[0], globals.visuals.distance_color[1], globals.visuals.distance_color[2], globals.visuals.distance_color[3] }, true);
                        }
                    }
                }

                if (globals.visuals.weapon_name) {
                    uint64_t pedweaponmanager = cPed + Address::Get()->m_pWeaponManager;
                    if (pedweaponmanager)
                    {
                        uint64_t pedweaponinfo = *(uint64_t*)(pedweaponmanager + 0x20);
                        if (pedweaponinfo)
                        {
                            if (globals.visuals.distance)
                            {
                                if (globals.visuals.healthposition == 1 || globals.visuals.armorposition == 1) {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y + 23), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                                else if (globals.visuals.healthposition == 1 && globals.visuals.armorposition == 1) {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y + 36), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                                else if (globals.visuals.healthposition == 0 && globals.visuals.armorposition == 0) {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y + 22), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                                else {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                            }
                            else
                            {
                                if (globals.visuals.healthposition == 1) {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y + 8), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                                else if (globals.visuals.armorposition == 1) {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y + 8), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                                else if (globals.visuals.healthposition == 1 && globals.visuals.armorposition == 1) {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y + 24), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                                else if (globals.visuals.healthposition == 0 && globals.visuals.armorposition == 0) {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y + 10), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                                else {
                                    Graphics::Get()->DrawTextA(ImGui::GetFont(), this->m_get_weapon_name(pedweaponinfo), ImVec2(foot.x, foot.y), globals.visuals.text_size, ImColor{ globals.visuals.weapon_color[0], globals.visuals.weapon_color[1], globals.visuals.weapon_color[2], globals.visuals.weapon_color[3] }, true);
                                }
                            }
                        }
                    }
                }
            }

            if (is_player) ++drawn_players; else ++drawn_npcs;
        }
    }

    
    if (globals.visuals.veh_activate && Address::Get()->s_mVehiclePool) {
        uint64_t pedfactory = *(uint64_t*)(Address::Get()->m_sPedFactory);
        if (!pedfactory) return;
        uint64_t localplayer = *(uint64_t*)(pedfactory + 0x8);
        if (!localplayer) return;

        Vector3 lp = *(Vector3*)(localplayer + 0x90);
        auto vehPoolPtrPtr = Address::Get()->s_mVehiclePool; 
        if (!vehPoolPtrPtr || !*vehPoolPtrPtr || !**vehPoolPtrPtr) return;
        auto* vehPool = **vehPoolPtrPtr;
        
        std::vector<uint64_t> worklist;
        worklist.reserve(vehPool->m_item_count);
        for (auto vehAddr : *vehPool) {
            if (vehAddr) worklist.push_back((uint64_t)vehAddr);
        }
        struct Item { uint64_t addr; float dist; };
        std::vector<Item> items;
        items.reserve(worklist.size());
        for (auto a : worklist) {
            Vector3 pos = *(Vector3*)(a + 0x90);
            Vector3 d = (lp - pos);
            float dist = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z);
            items.push_back({ a, dist });
        }
        
        if (globals.visuals.veh_max_display > 0 && (int)items.size() > globals.visuals.veh_max_display) {
            items.resize(globals.visuals.veh_max_display);
        }

        for (const auto& it : items) {
            uint64_t veh = it.addr;
            float dist = it.dist;
            if (!veh) continue;
            
            Vector3 pos = *(Vector3*)(veh + 0x90);
            if (globals.visuals.veh_view_distance > 0) {
                
                if (dist > (float)globals.visuals.veh_view_distance) continue;
            }

            ImVec2 center = Core::Get()->W2S(pos);
            if (!Graphics::Get()->IsOnScreen(center)) continue;

            
            float box_h = ImClamp(600.0f / (dist + 1.0f), 14.0f, 180.0f);
            float box_w = box_h * 1.6f;
            ImVec2 tl(center.x - box_w * 0.5f, center.y - box_h * 0.5f);
            ImVec2 br(center.x + box_w * 0.5f, center.y + box_h * 0.5f);

            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            float alpha_mul = 1.0f;
            if (globals.visuals.veh_fade_with_distance && globals.visuals.veh_view_distance > 0) {
                float t = 1.0f - ImSaturate(dist / (float)globals.visuals.veh_view_distance);
                alpha_mul = ImClamp(t, 0.15f, 1.0f);
            }

            if (globals.visuals.veh_box) {
                ImVec4 c4(globals.visuals.veh_boxcolor[0], globals.visuals.veh_boxcolor[1], globals.visuals.veh_boxcolor[2], globals.visuals.veh_boxcolor[3]);
                c4.w *= alpha_mul;
                ImU32 col = ImGui::GetColorU32(c4);
                if (globals.visuals.veh_boxstyle == 0) {
                    dl->AddRect(tl, br, col, 0, 0, (float)globals.visuals.veh_box_thickness);
                } else if (globals.visuals.veh_boxstyle == 1) { 
                    dl->AddRect(tl, br, col, 0, 0, (float)globals.visuals.veh_box_thickness);
                    dl->AddRectFilled(tl, br, ImGui::GetColorU32(ImVec4(0,0,0,0.27f * alpha_mul)));
                } else if (globals.visuals.veh_boxstyle == 2) {
                    float w = br.x - tl.x; float h = br.y - tl.y; float cx = tl.x; float cy = tl.y;
                    Graphics::Get()->DrawCornerBox(cx, cy, w, h, globals.visuals.veh_box_thickness, 0.25f, col);
                }
            }

            
            if (globals.visuals.veh_snapline) {
                ImVec2 screen_center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y);
                ImVec4 lc = ImVec4(1,1,1,0.7f * alpha_mul);
                Graphics::Get()->DrawLine(screen_center, center, ImGui::GetColorU32(lc), globals.visuals.veh_snapline_thickness);
            }

            
            if (globals.visuals.veh_names || globals.visuals.veh_distance) {
                std::string label = "Vehicle";
                uint64_t model_info = *(uint64_t*)(veh + 0x20);
                if (model_info) {
                    unsigned int hash = *(uint32_t*)(model_info + 0x18);
                    
                    extern const std::string& LookupVehNameByHash(unsigned int);
                    if (globals.visuals.veh_names) {
                        const std::string& name = LookupVehNameByHash(hash);
                        if (!name.empty()) label = name; else label.clear();
                    } else { label.clear(); }
                }
                if (globals.visuals.veh_distance) {
                    char buf[32]; _snprintf_s(buf, _TRUNCATE, "%dm", (int)dist);
                    if (!label.empty()) label += " ";
                    label += buf;
                }
                ImVec4 nc(globals.visuals.veh_namecolor[0], globals.visuals.veh_namecolor[1], globals.visuals.veh_namecolor[2], globals.visuals.veh_namecolor[3]);
                nc.w *= alpha_mul;
                ImU32 name_col = ImGui::GetColorU32(nc);
                ImVec2 text_pos;
                switch (globals.visuals.veh_label_pos) {
                default:
                case 0: text_pos = ImVec2(tl.x + (br.x - tl.x) * 0.5f, tl.y - 14); break; 
                case 1: text_pos = ImVec2(center.x, center.y); break; 
                case 2: text_pos = ImVec2(tl.x + (br.x - tl.x) * 0.5f, br.y + 4); break; 
                }
                if (globals.visuals.veh_label_bg) {
                    ImVec2 sz = ImGui::CalcTextSize(label.c_str());
                    ImVec2 pmin(text_pos.x - sz.x * 0.5f - 4, text_pos.y - 2);
                    ImVec2 pmax(text_pos.x + sz.x * 0.5f + 4, text_pos.y + sz.y + 2);
                    dl->AddRectFilled(pmin, pmax, ImGui::GetColorU32(ImVec4(0,0,0,0.5f * alpha_mul)), 3.0f);
                }
                Graphics::Get()->DrawTextA(ImGui::GetFont(), label, text_pos, globals.visuals.veh_text_size, name_col, true);

                if (globals.visuals.veh_velocity_line && Address::Get()->m_pVelocity) {
                    Vector3 vel = *(Vector3*)(veh + Address::Get()->m_pVelocity);
                    Vector3 tip3 = pos + (vel * 0.75f);
                    ImVec2 tip = Core::Get()->W2S(tip3);
                    ImVec4 vc = ImVec4(0.3f, 0.8f, 1.0f, 0.8f * alpha_mul);
                    Graphics::Get()->DrawLine(center, tip, ImGui::GetColorU32(vc), 2.0f);
                }
            }
        }
    }
}
