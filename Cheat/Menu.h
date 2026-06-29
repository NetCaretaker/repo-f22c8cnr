#pragma once
#include "../Main/includes/includes.h"

class Menu {
private:
    static Menu* s_pSingleton;
    Menu() = default;

public:
    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;

    static Menu* Get();
    void Load();
    void LoadImGui();
    static void RecoverImGuiStack();
};