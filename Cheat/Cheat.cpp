#include "Cheat.h"
#include "../MinHook.h"
#include "../Main/encrypt/skStr.h"

Cheat* Cheat::s_pSingleton = nullptr;

void Cheat::OnFrame(std::function<void()> function) {

	function();

	
	if (globals.visuals.watermark)
	{
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		static float pulse = 0.0f;
		static bool pulse_dir = true;
		const float dt = ImGui::GetIO().DeltaTime;
		pulse += (pulse_dir ? 1.0f : -1.0f) * dt * 1.5f;
		if (pulse >= 1.0f) { pulse = 1.0f; pulse_dir = false; }
		if (pulse <= 0.0f) { pulse = 0.0f; pulse_dir = true; }

		static char text[128];
		static ImVec2 cached_text_size(0, 0);
		static int last_shown_fps = -1;
		const ImGuiIO& io = ImGui::GetIO();
		int fps_display = g_last_present_fps > 0 ? g_last_present_fps : (int)io.Framerate;
		if (fps_display != last_shown_fps) {
			last_shown_fps = fps_display;
			snprintf(text, sizeof(text), "%s | Fps: %d", sk("discord.gg/nidevwz").decrypt(), fps_display);
			ImFont* font_tmp = ImGui::GetFont();
			float size_tmp = font_tmp->FontSize;
			cached_text_size = font_tmp->CalcTextSizeA(size_tmp, FLT_MAX, 0.0f, text);
		}

		ImVec2 margin = ImVec2(0.0f, 16.0f);
		ImFont* font = ImGui::GetFont();
		float font_size = font->FontSize;

		ImVec2 pad = ImVec2(12.0f, 7.0f);
		
		ImVec2 pos = ImVec2((display.x - (cached_text_size.x + pad.x * 2.0f)) * 0.5f,
						 margin.y);
		
		pos.x = floorf(pos.x + 0.5f);
		pos.y = floorf(pos.y + 0.5f);
		ImVec2 rect_min = pos;
		ImVec2 rect_max = ImVec2(pos.x + cached_text_size.x + pad.x * 2.0f, pos.y + cached_text_size.y + pad.y * 2.0f);

		ImDrawList* bg = ImGui::GetBackgroundDrawList();
		ImVec4 accent = ImGui::GetStyle().Colors[ImGuiCol_Scheme];
		float alpha_bg = 0.22f + 0.08f * pulse;
		float alpha_border = 0.45f + 0.25f * pulse;

		
		int fps_simple = fps_display > 0 ? fps_display : (int)io.Framerate;
		bool simple = (fps_simple < 75.0f);
		ImU32 text_shadow = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.65f));
		ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
		ImVec2 text_pos = ImVec2(pos.x + pad.x, pos.y + pad.y);
		text_pos.x = floorf(text_pos.x + 0.5f);
		text_pos.y = floorf(text_pos.y + 0.5f);
		if (simple) {
			bg->AddText(font, font_size, ImVec2(text_pos.x + 1, text_pos.y + 1), text_shadow, text);
			bg->AddText(font, font_size, text_pos, text_col, text);
		}
		else {
			
			bg->AddRectFilled(rect_min, rect_max, ImGui::GetColorU32(ImVec4(0.06f, 0.06f, 0.07f, 0.80f)), 6.0f);
			
			ImU32 accent_col = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, alpha_bg));
			bg->AddRectFilled(ImVec2(rect_min.x, rect_min.y), ImVec2(rect_max.x, rect_min.y + 2.0f), accent_col, 6.0f, 0);
			
			bg->AddRect(rect_min, rect_max, ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, alpha_border)), 6.0f, 0, 1.0f);
			
			bg->AddText(font, font_size, ImVec2(text_pos.x + 1, text_pos.y + 1), text_shadow, text);
			bg->AddText(font, font_size, text_pos, text_col, text);
		}
	}

	this->InitializeFeatures();
}

void Cheat::InitializeFeatures() {
	static Aimbot* m_sAimbot = new Aimbot();
    static Visuals* m_sVisuals = new Visuals();
    static Self* m_sSelf = new Self();
    static Vehicle* m_sVehicle = new Vehicle();
    static Weapon* m_sWeapon = new Weapon();

	m_sAimbot->Initialize();
    m_sVisuals->Initialize();
    m_sSelf->Initialize();
    m_sSelf->Freecam();
    m_sVehicle->Initialize();
    m_sWeapon->Initialize();

    
}
