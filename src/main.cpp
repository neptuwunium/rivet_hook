// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "runtime.hpp"

auto APIENTRY
DllMain(HMODULE, const DWORD reason, LPVOID) -> BOOL {
	HMODULE mod;
	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, nullptr, &mod)) {
		return TRUE;
	}

	char module_name[MAX_PATH] = {0};
	if (GetModuleFileNameA(mod, module_name, sizeof(module_name)) > 0) {
		std::string module_name_str = std::string(module_name);
		if (module_name_str.ends_with("/crs-handler.exe") || module_name_str.ends_with("/crs-video.exe")) {
			return TRUE;
		}
	}

	if (reason == DLL_PROCESS_ATTACH) {
		rivet_hook::runtime::init();
		rivet_hook::g_output.flush();
	} else if (reason == DLL_PROCESS_DETACH) {
		rivet_hook::runtime::fini();
	}

	return TRUE;
}
