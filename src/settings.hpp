// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <map>
#include <string>
#include <vector>

namespace rivet_hook {
	constexpr static auto settings_name = R"(.\rivet.toml)";

	struct Settings {
		static constexpr auto utility_group = "utility";
		bool suppress_crash_handler = true;
		bool attach_context_log = false;
		bool attach_log = false;
		bool unpause_focus = false;

		static constexpr auto ddl_group = "ddl";
		bool list_versions = false;
		bool dump_ddl = false;
		bool debug_ddl = false;

		static constexpr auto renderdoc_group = "renderdoc";
		bool load_renderdoc = false;
		std::string renderdoc_path { "renderdoc.dll" };

		static constexpr auto assets_group = "assets";
		bool enable_asset_loader = true;
		std::vector<std::string> asset_paths = { "mods/default" };
		bool force_legacy_textures = true;

		static constexpr auto log_group = "log";
		bool log_cohtml = false;
		bool log_paths = false;
		bool log_loose_io = false;
		bool log_asset_opens = false;
		bool log_asset_ids = false;
		bool log_mod_access = false;
		bool log_mod_state = false;
		bool log_hook_state = false;

		static constexpr auto addr_cache_group = "address_cache";
		std::string fingerprint;
		std::map<std::string, std::vector<intptr_t>> addresses;

		static auto
		load() -> Settings;
		auto
		save() const -> void;
	};
} // namespace rivet_hook
