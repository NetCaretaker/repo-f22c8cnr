#pragma once

#include "../Main/includes/includes.h"

class Cheat {
public:
	static Cheat* Get() {
		if (s_pSingleton == nullptr)
			s_pSingleton = new Cheat();
		return s_pSingleton;
	}
	static Cheat* s_pSingleton;

	void OnFrame(std::function<void()> function);

	void InitializeFeatures();
};