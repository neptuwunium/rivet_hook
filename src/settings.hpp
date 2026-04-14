// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <map>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include "windows.h"

#include <cstdint>

namespace rivet_hook {
	constexpr static auto settings_name = R"(.\rivet.toml)";

	struct Settings {
		struct Utility {
			bool suppress_crash_handler = true;
			bool attach_context_log = false;
			bool attach_log = false;
			bool unpause_focus = false;
		} utility;

		struct Overlay {
			bool enabled = true;
			int toggle_key = VK_F3;
			int spawn_debug_actor_key = VK_F4;
			int release_key = VK_NUMPAD5;
		} overlay;

		struct DDL {
			bool dump_versions = false;
			bool dump_ddl = false;
			bool dump_components = false;
			bool debug_ddl = false;
		} ddl;

		struct RenderDoc {
			bool enabled = false;
			std::string dll_path { "renderdoc.dll" };
		} renderdoc;

		struct Assets {
			bool enabled = true;
			std::vector<std::string> paths = { "mods/default" };
			bool disable_dstorage = true;
			bool log = false;
			bool verbose = false;
		} assets;

		struct Log {
			bool cohtml = false;
			bool paths = false;
			bool loose_io = false;
			bool asset_io = false;
			bool id = false;
			bool pointers = false;
		} log;

		struct AddressCache {
			std::string fingerprint;
			std::map<uint64_t, std::vector<intptr_t>> addresses;
		} address_cache;

		static auto
		load() -> Settings;
		auto
		save() const -> void;
	};
} // namespace rivet_hook
