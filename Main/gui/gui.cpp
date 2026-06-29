#include "gui.h"
#include <map>

using namespace ImGui;

bool ui::tab(int num) {
    auto label = tabs[num].m_icon; 

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    ImVec2 pos = window->DC.CursorPos;

    const float tab_height = 30.0f;
    const float tab_w = ui::tab_width;
    const ImRect rect(pos, ImVec2(pos.x + tab_w, pos.y + tab_height));
    
    ItemSize(rect, style.FramePadding.y);
    if (!ItemAdd(rect, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(rect, id, &hovered, &held, NULL);

    bool selected = num == next_tab;

    if (pressed && !selected) {
        content_anim_dest = 0.f;
        next_tab = num;
    }

    static std::map<ImGuiID, float> fade_alpha;
    float& sel_a = fade_alpha[id];
    float target = selected ? 1.0f : 0.0f;
    sel_a = ImLerp(sel_a, target, GetIO().DeltaTime * 12.0f);

    ImVec4 accent = GetStyle().Colors[ImGuiCol_Scheme];
    float text_alpha = selected ? 1.0f : (hovered ? 0.7f : 0.4f);
    ImU32 text_col = GetColorU32(ImVec4(1.f, 1.f, 1.f, text_alpha));

    (void)hovered;

    ImVec2 text_pos = ImVec2(rect.Min.x + (rect.GetWidth() - label_size.x) * 0.5f, rect.Min.y + (rect.GetHeight() - label_size.y) * 0.5f);
    window->DrawList->AddText(text_pos, text_col, label);

    if (sel_a > 0.01f) {
        float line_w = 16.0f * sel_a;
        float line_x = rect.Min.x + (rect.GetWidth() - line_w) * 0.5f;
        float line_y = rect.Max.y - 1.0f;
        window->DrawList->AddRectFilled(
            ImVec2(line_x, line_y),
            ImVec2(line_x + line_w, line_y + 2.0f),
            GetColorU32(ImVec4(accent.x, accent.y, accent.z, sel_a)),
            1.0f
        );
    }

    return pressed;
}

bool ui::subtab(int num) {
    auto label = tabs[cur_page].m_subtabs[num];

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    ImVec2 pos = window->DC.CursorPos;

    const float tab_height = 24.0f;
    const float tab_w = 90.0f;
    const ImRect rect(pos, ImVec2(pos.x + tab_w, pos.y + tab_height));
    
    ItemSize(ImVec4(rect.Min.x, rect.Min.y, rect.Max.x + 2.f, rect.Max.y), style.FramePadding.y);
    if (!ItemAdd(rect, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(rect, id, &hovered, &held, NULL);

    bool selected = num == tabs[cur_page].next_page;

    if (pressed && !selected) {
        content_anim2_dest = 0.f;
        tabs[cur_page].next_page = num;
    }

    static std::map<ImGuiID, float> subtab_sel_anim;
    float& sel_anim = subtab_sel_anim[id];
    sel_anim = ImLerp(sel_anim, selected ? 1.0f : 0.0f, GetIO().DeltaTime * 12.0f);

    ImVec4 accent = GetStyle().Colors[ImGuiCol_Scheme];
    float text_alpha = selected ? 0.95f : (hovered ? 0.55f : 0.35f);
    ImU32 text_col = GetColorU32(ImVec4(1.f, 1.f, 1.f, text_alpha));

    // No background fill on hover/selected — text opacity only
    (void)hovered;

    // Underline indicator (like tabs)
    if (sel_anim > 0.01f) {
        float line_w = 16.0f * sel_anim;
        float line_x = rect.Min.x + (rect.GetWidth() - line_w) * 0.5f;
        float line_y = rect.Max.y - 1.0f;
        window->DrawList->AddRectFilled(
            ImVec2(line_x, line_y),
            ImVec2(line_x + line_w, line_y + 2.0f),
            GetColorU32(ImVec4(accent.x, accent.y, accent.z, sel_anim)), 1.0f);
    }

    ImVec2 text_pos = ImVec2(rect.Min.x + (rect.GetWidth() - label_size.x) * 0.5f, rect.Min.y + (rect.GetHeight() - label_size.y) * 0.5f);
    window->DrawList->AddText(text_pos, text_col, label, FindRenderedTextEnd(label));

    SetCursorPosY(GetCursorPosY() + rect.GetHeight() + style.ItemSpacing.y - 1.0f);

    return pressed;
}

int rotation_start_index;
void ui::rotate_start()
{
    rotation_start_index = GetWindowDrawList()->VtxBuffer.Size;
}

ImVec2 ui::text_size(fonts_ font, float size, const char* text) {
    if (!fonts[font].get(size))
        return ImVec2{ 0, 0 };

    return fonts[font].get(size)->CalcTextSizeA(size, FLT_MAX, -1, text, FindRenderedTextEnd(text));
}

ImVec2 ui::rotation_center()
{
    ImVec2 l(FLT_MAX, FLT_MAX), u(-FLT_MAX, -FLT_MAX);

    const auto& buf = GetWindowDrawList()->VtxBuffer;
    for (int i = rotation_start_index; i < buf.Size; i++)
        l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);

    return ImVec2((l.x + u.x) / 2, (l.y + u.y) / 2);
}

void ui::rotate_end(float rad, ImVec2 center)
{
    float s = sin(rad), c = cos(rad);
    center = ImRotate(center, s, c) - center;

    auto& buf = GetWindowDrawList()->VtxBuffer;
    for (int i = rotation_start_index; i < buf.Size; i++)
        buf[i].pos = ImRotate(buf[i].pos, s, c) - center;
}

void ui::handle_alpha_anim() {
    content_anim = ImLerp(content_anim, content_anim_dest, 45.f * GetIO().DeltaTime);
    content_anim2 = ImLerp(content_anim2, content_anim2_dest, 45.f * GetIO().DeltaTime);

    if (content_anim < 0.001f) {
        content_anim_dest = 1.f;
        cur_page = next_tab;
    }

    if (content_anim2 < 0.001f) {
        content_anim2_dest = 1.f;
        tabs[cur_page].cur_subtab = tabs[cur_page].next_page;
    }
}

void ui::render_page() {
    if (tabs[cur_page].pages.size() == 0 || tabs[cur_page].pages.size() <= tabs[cur_page].cur_subtab)
        return;

    if (next_tab != cur_page) {
        GetCurrentWindow()->Scroll.y = 0;
        GetCurrentWindow()->ScrollTarget.y = 0;
    }

    tabs[cur_page].pages[tabs[cur_page].cur_subtab]();
}

void ui::add_page(int tab, std::function< void() > code) {
    tabs[tab].pages.emplace_back(code);
}

void ui::begin_child(const char* name, ImVec2 size) {
    auto style = GImGui->Style;

    PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
    BeginChild(
        std::string(name).append("main").c_str(),
        CalcItemSize(size, GetWindowWidth() / 2 - style.ItemSpacing.x / 2 - style.WindowPadding.x, 0),
        0,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground 
    );

    
    
    std::string _disp = name ? std::string(name) : std::string();
    const size_t _hashPos = _disp.find("##");
    const std::string _visible = (_hashPos == std::string::npos) ? _disp : _disp.substr(0, _hashPos);
    if (!_visible.empty()) {
        ImVec2 title_pos = GetCursorScreenPos();
        ImVec2 title_sz = CalcTextSize(_visible.c_str());
        ImVec4 accent = GetStyle().Colors[ImGuiCol_Scheme];
        GetWindowDrawList()->AddText(title_pos, GetColorU32(ImGuiCol_Text), _visible.c_str());
        GetWindowDrawList()->AddRectFilled(
            ImVec2(title_pos.x, title_pos.y + title_sz.y + 7.0f),
            ImVec2(title_pos.x + 28.0f, title_pos.y + title_sz.y + 10.0f),
            GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.95f)),
            2.0f
        );
        GetWindowDrawList()->AddRectFilled(
            ImVec2(title_pos.x + 30.0f, title_pos.y + title_sz.y + 7.0f),
            ImVec2(title_pos.x + CalcItemSize(size, GetWindowWidth() / 2 - style.ItemSpacing.x / 2 - style.WindowPadding.x, 0).x - 6.0f, title_pos.y + title_sz.y + 8.0f),
            GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)),
            1.0f
        );
        SetCursorPosY(28);
    }
    PushStyleVar(ImGuiStyleVar_WindowPadding, { 18, 14 });
    BeginChild(
        name,
        { -1, size.y == 0 ? size.y : -1 },
        0,
        ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground
    );
    PopStyleVar();
    PushStyleVar(ImGuiStyleVar_ItemSpacing, { 12, 8 });

    
    ImVec4 accent = GetStyle().Colors[ImGuiCol_Scheme];
    PushStyleColor(ImGuiCol_CheckMark, accent);
    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(accent.x, accent.y, accent.z, 0.35f));
    PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(accent.x, accent.y, accent.z, 0.55f));
    PushStyleColor(ImGuiCol_SliderGrab, accent);
    PushStyleColor(ImGuiCol_SliderGrabActive, accent);
    
    
    PushStyleColor(ImGuiCol_Border, ImVec4(accent.x, accent.y, accent.z, 0.3f));
    PushStyleColor(ImGuiCol_Scheme, accent);

    ImVec2 child_min = GetWindowPos();
    ImVec2 child_max = ImVec2(child_min.x + GetWindowSize().x, child_min.y + GetWindowSize().y);
    GetWindowDrawList()->AddRectFilled(child_min, child_max, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.02f)), 8.0f);
    GetWindowDrawList()->AddRect(child_min, child_max, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.04f)), 8.0f, 0, 1.0f);
}



void ui::end_child() {
    
    PopStyleColor(8);
    PopStyleVar();
    EndChild();
    EndChild();
    PopStyleVar();
}

bool ui::color_btn(const char* str_id, float col[4], ImVec2 size) {
    auto window = GetCurrentWindow();
    auto& style = GetStyle();
    auto id = window->GetID(str_id);

    ImVec2 p = window->DC.CursorPos;
    ImRect bb(p, p + size);

    ItemSize(bb);
    ItemAdd(bb, id);

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    bool value_changed = false;

    struct s {
        float anim = 0;
        bool open = false;
    }; auto& obj = anim_obj(str_id, 110, s{ });

    if (pressed) {
        obj.open = !obj.open;
        value_changed = true;
    }

    obj.anim = anim(obj.anim, 0.f, 1.f, obj.open);

    window->DrawList->AddRectFilled(bb.Min, bb.Max, ImColor{ col[0], col[1], col[2], style.Alpha }, 2);

    window->DrawList->AddRectFilled({ bb.GetCenter().x - bb.GetWidth() / 6, bb.Min.y }, { bb.GetCenter().x + bb.GetWidth() / 6, bb.GetCenter().y }, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha * (1.f - col[3]) * 0.6f });
    window->DrawList->AddRectFilled({ bb.Min.x, bb.GetCenter().y }, { bb.Min.x + size.x / 3, bb.Max.y }, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha * (1.f - col[3]) * 0.6f }, 2, ImDrawFlags_RoundCornersBottomLeft);
    window->DrawList->AddRectFilled({ bb.Max.x - size.x / 3, bb.GetCenter().y }, bb.Max, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha * (1.f - col[3]) * 0.6f }, 2, ImDrawFlags_RoundCornersBottomRight);

    if (obj.anim > 0.01f) {
        PushStyleColor(ImGuiCol_WindowBg, GetColorU32(ImGuiCol_FrameBg));
        PushStyleVar(ImGuiStyleVar_Alpha, obj.anim);
        PushStyleVar(ImGuiStyleVar_WindowRounding, style.FrameRounding);
        PushStyleVar(ImGuiStyleVar_ItemSpacing, { 10, 10 });
        PushStyleVar(ImGuiStyleVar_WindowPadding, { 10, 10 });
        Begin(str_id, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
        {
            SetWindowPos({ bb.Max.x - 230, bb.Max.y + 10 + 20 * (1.f - obj.anim) });

            

            if (!IsWindowHovered() && IsMouseClicked(0) && !hovered) obj.open = false;

            value_changed = color_picker(str_id, col);
        }
        End();
        PopStyleVar(4);
        PopStyleColor();
    }

    return value_changed;
}

bool ui::color_edit(const char* label, float col[4]) {
    GetWindowDrawList()->AddText(GetCurrentWindow()->DC.CursorPos + ImVec2{ 0, 9 - GImGui->FontSize / 2 }, GetColorU32(ImGuiCol_Text), label, FindRenderedTextEnd(label));
    Dummy({ 0, 0 });
    SameLine(GetWindowWidth() - 16 - 24);
    return color_btn(label, col, { 24, 18 });
}

bool ui::radio_button(const char* label, bool selected) {
    auto window = GetCurrentWindow();
    auto& style = GetStyle();
    auto id = window->GetID(label);

    float r = 8.5f;
    ImVec2 size(r * 2 + style.ItemInnerSpacing.x + CalcTextSize(label, 0, 1).x, r * 2);
    ImVec2 p = window->DC.CursorPos;
    ImRect bb(p, p + size);

    ItemSize(bb);
    ItemAdd(bb, id);

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    struct s {
        float anim = 0;
        float hover = 0;
        float selected = 0;
        ImColor col = 0;
        ImColor border_col = 0;
    }; auto& obj = anim_obj(label, 56, s{ });

    obj.anim = anim(obj.anim, 0.f, 1.f, hovered || selected);
    obj.selected = anim(obj.selected, 0.f, 1.f, selected);
    obj.hover = anim(obj.hover, 0.f, 1.f, hovered);
    obj.col = col_anim(GetColorU32(ImGuiCol_TextDisabled), GetColorU32(ImGuiCol_Text), obj.anim);
    obj.border_col = col_anim(GetColorU32(ImGuiCol_Border), GetColorU32(ImGuiCol_TextDisabled), obj.anim);

    window->DrawList->AddCircle(bb.Min + r, r, obj.border_col, 36);
    window->DrawList->AddCircleFilled(bb.Min + r, (r - 4) * obj.selected, GetColorU32(ImGuiCol_Scheme, obj.selected), 36);

    window->DrawList->AddText({ bb.Min.x + r * 2 + style.ItemInnerSpacing.x, bb.GetCenter().y - CalcTextSize(label, 0, 1).y / 2 }, obj.col, label, FindRenderedTextEnd(label));

    return pressed;
}

void ui::radio(std::vector< const char* > items, int* v) {
    for (int i = 0; i < items.size(); ++i) {
        if (radio_button(items[i], i == *v))
            *v = i;
    }
}


bool ui::binder(const char* name, c_key* bind) {

    ImGuiWindow* window = GetCurrentWindow();
    ImGuiContext& g = *GImGui;

    PushStyleVar(ImGuiStyleVar_FramePadding, { 10, 7 });

    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(name);
    ImGuiIO& io = g.IO;

    const ImVec2 key_size = text_size(fontb, 12, g.ActiveId == id ? "???" : KEY_NAMES[bind->key]);

    struct s {
        float anim;
        float hover;
        float active;
        ImColor col;
        float bind_w;

        bool popup = false;
        float popup_anim = 0.f;
    }; auto& obj = anim_obj(name, 6, s{ 0.f, 0.f, 0.f, 0, 0.f });

    obj.bind_w = ImLerp(obj.bind_w, key_size.x, GetIO().DeltaTime * 18.f);

    const ImRect total_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2{ CalcTextSize(name, 0, 1).x > 0 ? CalcItemWidth() : obj.bind_w + g.Style.FramePadding.x * 2, key_size.y + g.Style.FramePadding.y * 2 });
    const ImRect bb(total_bb.Max - ImVec2{ obj.bind_w + g.Style.FramePadding.x * 2, key_size.y + g.Style.FramePadding.y * 2 }, total_bb.Max);

    ItemSize(total_bb, style.FramePadding.y);
    ItemAdd(total_bb, id, &bb);

    const bool hovered = ItemHoverable(bb, id);

    const bool SHOULD_EDIT = hovered && io.MouseClicked[0];

    if (SHOULD_EDIT) {
        if (g.ActiveId != id) {
            memset(io.MouseDown, 0, sizeof(io.MouseDown));
            memset(io.KeysDown, 0, sizeof(io.KeysDown));
            bind->key = 58;
        }

        SetActiveID(id, window);
        FocusWindow(window);
    }
    else if (!hovered && io.MouseClicked[0] && g.ActiveId == id)
        ClearActiveID();

    bool value_changed = false;
    int key = bind->key;

    if (g.ActiveId == id) {
        for (auto i = 0; i < 5; i++) {
            if (io.MouseDown[i]) {
                switch (i) {
                case 0:
                    key = VK_LBUTTON;
                    break;
                case 1:
                    key = VK_RBUTTON;
                    break;
                case 2:
                    key = VK_MBUTTON;
                    break;
                case 3:
                    key = VK_XBUTTON1;
                    break;
                case 4:
                    key = VK_XBUTTON2;
                }
                value_changed = true;
                ClearActiveID();
            }
        }

        if (!value_changed) {
            for (auto i = VK_BACK; i <= VK_RMENU; i++) {
                if (io.KeysDown[i]) {
                    key = i;
                    value_changed = true;
                    ClearActiveID();
                }
            }
        }

        if (IsKeyPressedMap(ImGuiKey_Escape)) {
            bind->key = 0;
            ClearActiveID();
        }
        else
            bind->key = key;
    }

    std::string buf_display = "NONE";
    if (bind->key != 0 && g.ActiveId != id)
        buf_display = KEY_NAMES[bind->key];
    else if (g.ActiveId == id)
        buf_display = "???";

    obj.anim = anim(obj.anim, 0.f, 1.f, g.ActiveId == id || hovered);
    obj.active = anim(obj.active, 0.f, 1.f, g.ActiveId == id);
    obj.hover = anim(obj.hover, 0.f, 1.f, hovered);
    obj.col = col_anim(GetColorU32(ImGuiCol_TextDisabled), GetColorU32(ImGuiCol_Text), obj.anim);
    obj.popup_anim = anim(obj.popup_anim, 0.f, 1.f, obj.popup);

    window->DrawList->AddRect(bb.Min, bb.Max, col_anim(GetColorU32(ImGuiCol_Border), GetColorU32(ImGuiCol_Scheme), obj.anim), style.FrameRounding);

    window->DrawList->AddText(total_bb.Min + ImVec2{ 0, total_bb.GetHeight() / 2 - GImGui->FontSize / 2 }, GetColorU32(ImGuiCol_Text), name, FindRenderedTextEnd(name));

    window->DrawList->AddText(fonts[fontb].get(12), 12, bb.Min + style.FramePadding, obj.col, buf_display.c_str());

    if (hovered && IsMouseClicked(1)) {
        obj.popup = true;
    }

    const char* mods[]{
        "Toggle",
        "Hold",
        "Always on"
    };

    if (obj.popup_anim > 0.05f) {
        PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
        PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
        PushStyleVar(ImGuiStyleVar_FramePadding, { 12, 0 });
        PushStyleVar(ImGuiStyleVar_Alpha, obj.popup_anim);
        Begin(name, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
        {
            SetWindowSize({ 100, IM_ARRAYSIZE(mods) * 30 + style.FrameRounding * 2 });
            SetWindowPos({ bb.Max.x - 100, bb.Max.y + 10 + 10 * (1.f - obj.popup_anim) });

            GetWindowDrawList()->AddRectFilled(GetWindowPos(), GetWindowPos() + GetWindowSize(), GetColorU32(ImGuiCol_PopupBg), style.FrameRounding);

            

            if (!IsWindowHovered() && IsMouseClicked(0) && !hovered) obj.popup = false;

            SetCursorPosY(style.FrameRounding);
            BeginGroup();
            {
                PushFont(fonts[font].get(12));

                for (int i = 0; i < IM_ARRAYSIZE(mods); ++i) {
                    if (Selectable(mods[i], bind->mode == i, 0, { GetWindowWidth(), 30 })) {
                        bind->mode = i;
                        obj.popup = false;
                    }
                }

                PopFont();
            }
            EndGroup();
        }
        End();
        PopStyleVar(4);
    }

    PopStyleVar();

    return value_changed;
}

void ui::multi_select(const char* label, std::vector< multi_select_item >& items) {
    auto window = GetCurrentWindow();
    auto id = window->GetID(label);
    auto& style = GetStyle();

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 label_size = CalcTextSize(label, 0, 1);
    ImRect total_bb(pos, pos + ImVec2(CalcItemWidth(), GetFrameHeight()));
    ImRect bb(total_bb.Max - ImVec2(window->Size.x / 2, GetFrameHeight()), total_bb.Max);

    ItemSize(total_bb);
    ItemAdd(total_bb, id);

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    struct s {
        float anim;
        float rad;
        float hover;
        float open;
        ImColor col;
    }; auto& obj = anim_obj(label, 0, s{ 0.f, 0.f, 0.f, 0.f, 0 });

    if (pressed) obj.open = !obj.open;

    obj.anim = anim(obj.anim, 0.f, 1.f, obj.open);
    obj.rad = anim(obj.rad, IM_PI / 2, IM_PI * 1.5f, obj.open);
    obj.col = col_anim(GetColorU32(ImGuiCol_TextDisabled), GetColorU32(ImGuiCol_Text), obj.anim);

    RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), 1, style.FrameRounding);

    ui::rotate_start();
    RenderArrow(window->DrawList, { bb.Max.x - style.FramePadding.x - 11, bb.GetCenter().y - 2.5f }, obj.col, ImGuiDir_Down, 0.45f);
    ui::rotate_end(obj.rad, ui::rotation_center());

    std::string buf;

    buf.clear();
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i]) {
            buf += items[i].label;
            buf += ", ";
        }
    }

    if (!buf.empty()) {
        buf.resize(buf.size() - 2);
    }

    if (CalcTextSize(buf.c_str(), 0, 1).x > bb.GetWidth() - style.FramePadding.x * 3 - 10) {
        for (int i = 0; i < buf.size() - 1; ++i) {
            if (CalcTextSize(buf.substr(0, i + 1).c_str(), 0, 1).x > bb.GetWidth() - style.FramePadding.x * 3 - 10) {
                buf.resize(i);
                if (buf[buf.size() - 1] == ',') {
                    buf.resize(buf.size() - 1);
                }
                buf.append("..");
            }
        }
    }

    window->DrawList->AddText(total_bb.Min + ImVec2{ 0, bb.GetHeight() / 2 - GImGui->FontSize / 2 }, GetColorU32(ImGuiCol_Text), label, FindRenderedTextEnd(label));
    window->DrawList->AddText(bb.Min + style.FramePadding, GetColorU32(ImGuiCol_Text), buf.c_str(), FindRenderedTextEnd(buf.c_str()));

    if (obj.anim > 0.05f) {
        PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
        Begin(label, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
        {
            SetWindowSize({ bb.GetWidth(), (ImMin((int)items.size(), 5) * GetFrameHeight() + style.FrameRounding * 2) * obj.anim });
            SetWindowPos({ bb.Min.x, bb.Max.y });

            GetWindowDrawList()->AddRectFilled(GetWindowPos(), GetWindowPos() + GetWindowSize(), GetColorU32(ImGuiCol_ChildBg), style.FrameRounding, ImDrawFlags_RoundCornersBottom);
            GetWindowDrawList()->AddRect(bb.Min, GetWindowPos() + GetWindowSize(), GetColorU32(ImGuiCol_Border), style.FrameRounding, ImDrawFlags_RoundCornersBottom);
            window->DrawList->AddLine({ bb.Min.x, bb.Max.y - style.FrameRounding }, { bb.Min.x, bb.Max.y }, GetColorU32(ImGuiCol_Border));
            window->DrawList->AddLine({ bb.Max.x - 1, bb.Max.y - style.FrameRounding }, bb.Max - ImVec2{ 1, 0 }, GetColorU32(ImGuiCol_Border));

            BringWindowToFocusFront(GetCurrentWindow());
            BringWindowToDisplayFront(GetCurrentWindow());

            if (!IsWindowHovered() && IsMouseClicked(0) && !hovered) obj.open = false;

            SetCursorPosY(style.FrameRounding);
            BeginGroup();
            {
                for (int i = 0; i < items.size(); ++i) {
                    if (Selectable(items[i].label, items[i], 0, { GetWindowWidth(), GetFrameHeight() })) {
                        items[i].selected = !items[i];
                    }
                }

                if (items.size() > 5) {
                    Dummy({ 0, 9 });
                }
            }
            EndGroup();
        }
        End();
        PopStyleVar();
    }
}


void ui::styles() {
    auto& style = GImGui->Style;

    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.ChildRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;

    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;

    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(12, 10);
    style.ItemSpacing = ImVec2(10, 2);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 2.0f;
    style.GrabMinSize = 10.0f;

    style.DisplaySafeAreaPadding = ImVec2(4, 4);
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
}

void ui::colors() {
    ImVec4* colors = GImGui->Style.Colors;

    ImVec4 accent = ImVec4(0.40f, 0.42f, 0.96f, 1.0f);
    ImVec4 hover  = ImVec4(0.50f, 0.52f, 1.00f, 1.0f);
    ImVec4 active = ImVec4(0.60f, 0.62f, 1.00f, 1.0f);

    ImVec4 bgDark  = ImVec4(0.04f, 0.04f, 0.05f, 1.0f);
    ImVec4 bgLight = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
    ImVec4 panel   = ImVec4(0.07f, 0.07f, 0.08f, 1.0f);
    ImVec4 panel2  = ImVec4(0.09f, 0.09f, 0.10f, 1.0f);
    ImVec4 border  = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
    ImVec4 text    = ImVec4(0.88f, 0.88f, 0.92f, 1.0f);
    ImVec4 textDim = ImVec4(0.45f, 0.45f, 0.52f, 1.0f);

    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = textDim;
    colors[ImGuiCol_WindowBg] = bgDark;
    colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_PopupBg] = panel2;

    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.07f);

    colors[ImGuiCol_TitleBg] = bgDark;
    colors[ImGuiCol_TitleBgActive] = bgDark;
    colors[ImGuiCol_TitleBgCollapsed] = bgDark;

    colors[ImGuiCol_MenuBarBg] = bgDark;

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(1.f, 1.f, 1.f, 0.06f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.f, 1.f, 1.f, 0.10f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.f, 1.f, 1.f, 0.14f);

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = active;

    colors[ImGuiCol_Button] = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.07f);
    colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);

    colors[ImGuiCol_Header] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
    colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.07f);

    colors[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = active;

    colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_Tab] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
    colors[ImGuiCol_TabActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = hover;
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = hover;

    colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.20f);
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_NavHighlight] = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.5f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.4f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.6f);

    colors[ImGuiCol_Scheme] = accent;
}


bool ui::modern_button(const char* label, ImVec2 size) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    ImVec2 pos = window->DC.CursorPos;
    
    if (size.x == 0.0f) size.x = label_size.x + 24.0f;
    if (size.y == 0.0f) size.y = 32.0f;
    
    const ImRect bb(pos, pos + size);
    ItemSize(bb, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    static std::map<ImGuiID, float> button_anim;
    float& anim_val = button_anim[id];
    float target_anim = held ? 1.0f : (hovered ? 0.6f : 0.0f);
    anim_val = ImLerp(anim_val, target_anim, GetIO().DeltaTime * 14.0f);

    const float rounding = 6.0f;

    window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.03f + anim_val * 0.04f)), rounding);
    window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.08f + anim_val * 0.08f)), rounding, 0, 1.0f);

    ImVec2 text_pos = ImVec2(
        floorf(bb.Min.x + (bb.GetWidth() - label_size.x) * 0.5f + 0.5f),
        floorf(bb.Min.y + (bb.GetHeight() - label_size.y) * 0.5f + 0.5f)
    );
    ImU32 text_col = GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.6f + anim_val * 0.35f));
    window->DrawList->AddText(text_pos, text_col, label);

    return pressed;
}

void ui::modern_separator(const char* label) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    
    ImVec2 pos = window->DC.CursorPos;
    float separator_thickness = 1.0f;
    
    
    float w = window->Size.x - window->DC.CursorPos.x;
    ImVec2 label_pos = pos;
    ImVec2 separator_start = pos;
    ImVec2 separator_end = pos;
    
    if (label_size.x > 0.0f) {
        
        float label_width = label_size.x + style.ItemSpacing.x * 2.0f;
        separator_start.x = pos.x + label_width;
        separator_end.x = pos.x + w;
        label_pos.x = pos.x + style.ItemSpacing.x;
    } else {
        
        separator_end.x = pos.x + w;
    }
    
    
    ImVec4 accent = style.Colors[ImGuiCol_Scheme];
    ImU32 line_color = GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.3f));
    window->DrawList->AddLine(separator_start, separator_end, line_color, separator_thickness);
    
    
    if (label_size.x > 0.0f) {
        window->DrawList->AddText(label_pos, GetColorU32(ImGuiCol_Text), label);
    }
    
    
    SetCursorPosY(GetCursorPosY() + ImMax(label_size.y, separator_thickness) + style.ItemSpacing.y);
}

bool ui::slider_int(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags /*flags*/) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float full_w = CalcItemWidth();
    const float bar_h = 2.0f;
    const float grab_r = 4.5f;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImRect total_bb(pos, ImVec2(pos.x + full_w, pos.y + label_size.y + style.ItemInnerSpacing.y + grab_r * 2 + 2.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id))
        return false;

    ImVec2 label_pos = pos;
    window->DrawList->AddText(label_pos, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.55f)), label);

    ImVec2 bar_min = ImVec2(pos.x, pos.y + label_size.y + style.ItemInnerSpacing.y + grab_r - bar_h * 0.5f);
    ImVec2 bar_max = ImVec2(pos.x + full_w, bar_min.y + bar_h);
    ImRect bar_bb(bar_min, bar_max);

    bool hovered, held;
    bool pressed = ButtonBehavior(bar_bb, id, &hovered, &held);

    int old_v = *v;
    if (pressed || held) {
        float t_drag = ImClamp((g.IO.MousePos.x - bar_bb.Min.x) / ImMax(1.0f, bar_bb.GetWidth()), 0.0f, 1.0f);
        int new_v = (int)roundf(ImLerp((float)v_min, (float)v_max, t_drag));
        if (new_v != *v) { *v = new_v; MarkItemEdited(id); }
    }

    if (g.ActiveId == id) {
        int step = 1;
        if (IsKeyPressed(ImGuiKey_LeftArrow, true)) { *v = ImClamp(*v - step, v_min, v_max); MarkItemEdited(id); }
        if (IsKeyPressed(ImGuiKey_RightArrow, true)) { *v = ImClamp(*v + step, v_min, v_max); MarkItemEdited(id); }
    }

    float t = (*v - (float)v_min) / ImMax(1.0f, (float)(v_max - v_min));
    t = ImClamp(t, 0.0f, 1.0f);
    float fill_x = bar_bb.Min.x + t * bar_bb.GetWidth();

    ImVec4 accent = style.Colors[ImGuiCol_Scheme];

    window->DrawList->AddRectFilled(bar_bb.Min, bar_bb.Max, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.05f)), 1.0f);
    window->DrawList->AddRectFilled(bar_bb.Min, ImVec2(fill_x, bar_bb.Max.y), GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.7f)), 1.0f);

    ImVec2 grab_center = ImVec2(fill_x, (bar_bb.Min.y + bar_bb.Max.y) * 0.5f);
    window->DrawList->AddCircleFilled(grab_center, grab_r, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.92f)), 16);

    char buf[64];
    ImFormatString(buf, IM_ARRAYSIZE(buf), format ? format : "%d", *v);
    ImVec2 val_sz = CalcTextSize(buf);
    window->DrawList->AddText(ImVec2(total_bb.Max.x - val_sz.x, label_pos.y), GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.55f)), buf);

    return *v != old_v;
}

bool ui::slider_float(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags /*flags*/) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float full_w = CalcItemWidth();
    const float bar_h = 2.0f;
    const float grab_r = 4.5f;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImRect total_bb(pos, ImVec2(pos.x + full_w, pos.y + label_size.y + style.ItemInnerSpacing.y + grab_r * 2 + 2.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id))
        return false;

    ImVec2 label_pos = pos;
    window->DrawList->AddText(label_pos, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.55f)), label);

    ImVec2 bar_min = ImVec2(pos.x, pos.y + label_size.y + style.ItemInnerSpacing.y + grab_r - bar_h * 0.5f);
    ImVec2 bar_max = ImVec2(pos.x + full_w, bar_min.y + bar_h);
    ImRect bar_bb(bar_min, bar_max);

    bool hovered, held;
    bool pressed = ButtonBehavior(bar_bb, id, &hovered, &held);

    float old_v = *v;
    if (pressed || held) {
        float t_drag = ImClamp((g.IO.MousePos.x - bar_bb.Min.x) / ImMax(1.0f, bar_bb.GetWidth()), 0.0f, 1.0f);
        float new_v = ImLerp(v_min, v_max, t_drag);
        if (new_v != *v) { *v = new_v; MarkItemEdited(id); }
    }

    if (g.ActiveId == id) {
        float step = (v_max - v_min) * 0.01f;
        if (IsKeyPressed(ImGuiKey_LeftArrow, true)) { *v = ImClamp(*v - step, v_min, v_max); MarkItemEdited(id); }
        if (IsKeyPressed(ImGuiKey_RightArrow, true)) { *v = ImClamp(*v + step, v_min, v_max); MarkItemEdited(id); }
    }

    float t = (*v - v_min) / ImMax(1e-6f, (v_max - v_min));
    t = ImClamp(t, 0.0f, 1.0f);
    float fill_x = bar_bb.Min.x + t * bar_bb.GetWidth();

    ImVec4 accent = style.Colors[ImGuiCol_Scheme];

    window->DrawList->AddRectFilled(bar_bb.Min, bar_bb.Max, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.05f)), 1.0f);
    window->DrawList->AddRectFilled(bar_bb.Min, ImVec2(fill_x, bar_bb.Max.y), GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.7f)), 1.0f);

    ImVec2 grab_center = ImVec2(fill_x, (bar_bb.Min.y + bar_bb.Max.y) * 0.5f);
    window->DrawList->AddCircleFilled(grab_center, grab_r, GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.92f)), 16);

    char buf[64];
    ImFormatString(buf, IM_ARRAYSIZE(buf), format ? format : "%.3f", *v);
    ImVec2 val_sz = CalcTextSize(buf);
    window->DrawList->AddText(ImVec2(total_bb.Max.x - val_sz.x, label_pos.y), GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.55f)), buf);

    return *v != old_v;
}

bool ui::thin_slider_int(const char* label, int* v, int v_min, int v_max, const char* format) {
    return slider_int(label, v, v_min, v_max, format, 0);
}

bool ui::thin_slider_float(const char* label, float* v, float v_min, float v_max, const char* format) {
    return slider_float(label, v, v_min, v_max, format, 0);
}
