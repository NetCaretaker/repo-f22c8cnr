#pragma once

#include <d3d11.h>
#include <d3dx11tex.h>
#include "imgui/imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui_internal.h"
#include "imgui_tricks.hpp"
#include <functional>
#include <filesystem>
#include <../include\animations.hpp>
#include "colorpicker.h"
#include "fonts.h"

enum fonts_ {
    font,
    fontb,
    icons,
    size
};

inline std::vector< c_font > fonts( fonts_::size );

struct multi_select_item {
    const char* label;
    bool selected = false;

    multi_select_item( const char* _label ) : label{ _label } { };

    operator bool( ) const {
        return selected;
    }
};

struct c_tab {
    const char* m_icon;

    std::vector< std::function< void( ) > > pages;
    std::vector < const char* > m_subtabs;
    int cur_subtab = 0;
    int next_page = 0;

    c_tab( const char* icon, std::vector< const char* > subtabs ) {
        m_icon = icon;
        m_subtabs = subtabs;
    }
};

namespace ui {
    inline ImVec2 size{ 920, 560 };

    inline float content_anim = 0.f;
    inline float content_anim2 = 0.f;
    inline float content_anim_dest = 1.f;
    inline float content_anim2_dest = 1.f;
    inline int next_tab;
    inline int cur_page = 0;
    inline float tab_width = 100.0f;

    ImVec2 text_size(fonts_ font, float size, const char* text);
    ImVec2 rotation_center();

    void rotate_start( );
    void rotate_end( float rad, ImVec2 center );
    void handle_alpha_anim( );
    void render_page( );
    void add_page( int tab, std::function< void( ) > code );
    void styles( );
    void colors( );
    void radio( std::vector< const char* > items, int* v );
    void begin_child(const char* name, ImVec2 size = ImVec2{ 0, 0 });
    void end_child();
    void multi_select(const char* label, std::vector< multi_select_item >& items);

    bool tab(int num);
    bool subtab(int num);
    bool color_btn(const char* str_id, float col[4], ImVec2 size);
    bool color_edit(const char* label, float col[4]);
    bool radio_button(const char* label, bool selected);
    bool binder(const char* label, c_key* key);
    bool modern_button(const char* label, ImVec2 size = ImVec2(0, 0));
    void modern_separator(const char* label = nullptr);
    
    bool slider_int(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool slider_float(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    
    bool thin_slider_int(const char* label, int* v, int v_min, int v_max, const char* format = "%d");
    bool thin_slider_float(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f");

    inline std::vector< c_tab > tabs{
        { "Aimbot", { "Aimbot" } },
        { "Visuals", { "Entities", "Vehicles" }},
        { "Misc", { "Self", "Weapon", "Vehicle", "Teleport"}},
        { "Online", { "Player", "Vehicles" }},
        { "Lua", { "Resources", "Lua##editor" } },
            { "Settings", { "UI", "Configs" } },
    };
};
