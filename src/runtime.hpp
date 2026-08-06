// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <fstream>
#include <string_view>
#include <vector>

#include "settings.hpp"
#include "signature_types.hpp"

namespace rivet_hook {
	extern std::ofstream g_output;
	extern Settings g_settings;
	extern HMODULE g_game_module;
	extern HANDLE g_game_inited;

	constexpr std::string_view RIVET_VERSION = "1.2.0";

	auto
	load_rel_var(intptr_t ptr, int rel_address) -> void *;

	auto
	find_addresses(const hex_signature &signature) -> std::vector<intptr_t>;

	auto
	find_address(const hex_signature &signature, size_t limit = 1, int select = 0) -> intptr_t;

	auto
	create_hook(const std::string_view &name, LPVOID pointer, LPVOID detour, LPVOID *original) -> void;

	auto
	create_hook(const hex_signature &signature, LPVOID detour, LPVOID *original, size_t limit = 1, int select = 0) -> void;

	namespace runtime {
		auto
		init() -> void;
		auto
		fini() -> void;
	} // namespace runtime
} // namespace rivet_hook
