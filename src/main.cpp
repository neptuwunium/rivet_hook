// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "runtime.hpp"

auto APIENTRY
DllMain(HMODULE, const DWORD reason, LPVOID) -> BOOL {
	if (reason == DLL_PROCESS_ATTACH) {
		rivet_hook::runtime::init();
		rivet_hook::g_output.flush();
	} else if (reason == DLL_PROCESS_DETACH) {
		rivet_hook::runtime::fini();
	}

	return TRUE;
}
