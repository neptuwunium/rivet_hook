// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <format>
#include <fstream>

#include <toml.hpp>

#include "settings.hpp"
#include "runtime.hpp"

#define LOAD_SETTING_EX(group, type, name, var_name)                                 \
	if (tbl.contains(group) && tbl.at(group).is_table()) {                           \
		settings.name = toml::find_or<type>(tbl.at(group), var_name, settings.name); \
	}

#define LOAD_SETTING(group, type, name) LOAD_SETTING_EX(group, type, name, #name)

#define SAVE_SETTING_EX(group, name, var_name, comment) \
	tbl[group][var_name] = name;                        \
	tbl[group].at(var_name).comments().push_back(" " comment)

#define SAVE_SETTING(group, name, comment) SAVE_SETTING_EX(group, name, #name, comment)

namespace rivet_hook {
	constexpr auto addr_cache_group = "address_cache";

	auto valid_fingerprint(Settings &settings) -> bool {
		const auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(g_game_module);

		if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
			return false;
		}

		const auto headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<PBYTE>(g_game_module) + dos->e_lfanew);
		if (headers->Signature != IMAGE_NT_SIGNATURE) {
			return false;
		}

		if (const auto fingerprint = std::format("{}:{}#{}", headers->OptionalHeader.AddressOfEntryPoint, headers->OptionalHeader.SizeOfImage, headers->FileHeader.TimeDateStamp);
			fingerprint != settings.fingerprint) {
			g_output << "[rivet] game version mismatch, invalidating pointers\n";
			settings.fingerprint = fingerprint;
			return false;
		}

		return true;
	}

	auto
	Settings::load() -> Settings {
		Settings settings;

		try {
			const auto tbl = toml::parse(settings_name);

			LOAD_SETTING("utility", bool, suppress_crash_handler);
			LOAD_SETTING("utility", bool, attach_context_log);
			LOAD_SETTING("utility", bool, attach_log);

			LOAD_SETTING("ddl", bool, list_versions);
			LOAD_SETTING("ddl", bool, dump_ddl);
			LOAD_SETTING("ddl", bool, debug_ddl);

			LOAD_SETTING_EX("renderdoc", bool, load_renderdoc, "enabled");
			LOAD_SETTING_EX("renderdoc", std::string, renderdoc_path, "dll_path");

			LOAD_SETTING_EX("assets", bool, enable_asset_loader, "enabled");
			LOAD_SETTING_EX("assets", bool, log_mod_access, "log");
			LOAD_SETTING_EX("assets", bool, log_mod_state, "verbose");
			LOAD_SETTING_EX("assets", std::vector<std::string>, asset_paths, "paths");

			LOAD_SETTING_EX("log", bool, log_cohtml, "cohtml");
			LOAD_SETTING_EX("log", bool, log_paths, "paths");
			LOAD_SETTING_EX("log", bool, log_loose_io, "loose_io");
			LOAD_SETTING_EX("log", bool, log_asset_opens, "asset_io");
			LOAD_SETTING_EX("log", bool, log_asset_ids, "id");
			LOAD_SETTING_EX("log", bool, log_hook_state, "pointers");

			settings.fingerprint = toml::find_or<std::string>(tbl, "__GAME_ID__", settings.fingerprint);
			if (tbl.contains(addr_cache_group)) {
				if (auto addr_cache = tbl.at(addr_cache_group); addr_cache.is_table()) {
					for (auto &[original_key, values] : addr_cache.as_table()) {
						if (!values.is_array()) {
							continue;
						}

						std::string key = original_key;
						std::ranges::replace(key, '_', ' ');
						settings.addresses[key] = {};

						for (auto &value : values.as_array()) {
							settings.addresses[key].emplace_back(value.as_integer() + reinterpret_cast<intptr_t>(g_game_module));
						}
					}
				}
			}
		} catch (const std::exception &) {
			// ignored
		}

		if (!valid_fingerprint(settings)) {
			settings.addresses.clear();
		}

		return settings;
	}

	auto
	Settings::save() const -> void {
		toml::value tbl = toml::ordered_table();
		tbl["utility"] = toml::ordered_table();
		tbl["ddl"] = toml::ordered_table();
		tbl["renderdoc"] = toml::ordered_table();
		tbl["assets"] = toml::ordered_table();
		tbl["log"] = toml::ordered_table();
		tbl[addr_cache_group] = toml::ordered_table();

		SAVE_SETTING("utility", suppress_crash_handler, "disable the exception handler allowing for debuggers to attach without invoking the crash handler");
		SAVE_SETTING("utility", attach_context_log, "redirect the internal logger context state to rivet.log; disable by default for clutter reasons");
		SAVE_SETTING("utility", attach_log, "redirect the internal logger to rivet.log; disable by default because the same line is printed frequently");

		SAVE_SETTING("ddl", list_versions, "dumps versions to json; disable by default for clutter reasons");
		SAVE_SETTING("ddl", dump_ddl, "dumps DDL type structures to json; disable by default for clutter reasons");
		SAVE_SETTING("ddl", debug_ddl, "logs DDL type information; disable by default because log noise");

		SAVE_SETTING_EX("renderdoc", load_renderdoc, "enabled", "loads renderdoc.dll into the game; disable by default because it has issues with ReShade");
		SAVE_SETTING_EX("renderdoc", renderdoc_path, "dll_path", "path to renderdoc/dll");

		SAVE_SETTING_EX("assets", enable_asset_loader, "enabled", "enables loose asset loading");
		SAVE_SETTING_EX("assets", asset_paths, "paths", "list of paths to load assets from, order is priority. first entry is least priority.");
		SAVE_SETTING_EX("assets", log_mod_access, "log", "logs when mod files are accessed; disable by default because log noise");
		SAVE_SETTING_EX("assets", log_mod_state, "verbose", "logs the mod pipeline state; disable by default because log noise");

		SAVE_SETTING_EX("log", log_cohtml, "cohtml", "logs coherent ui url decode requests; disable by default because log noise");
		SAVE_SETTING_EX("log", log_paths, "paths", "logs asset paths as they are loaded; disable by default because log noise");
		SAVE_SETTING_EX("log", log_loose_io, "loose_io", "logs loose io; disable by default because log noise");
		SAVE_SETTING_EX("log", log_asset_opens, "asset_io", "logs asset paths as they are assets; disable by default because log noise");
		SAVE_SETTING_EX("log", log_asset_ids, "id", "logs asset ids as they are hashed; disable by default because log noise");
		SAVE_SETTING_EX("log", log_hook_state, "pointers", "logs pointer information; disable by default because log noise");

		tbl["__GAME_ID__"] = fingerprint;
		for (auto &[original_key, values] : addresses) {
			auto arr = toml::array();
			for (const auto value : values) {
				arr.emplace_back(value - reinterpret_cast<intptr_t>(g_game_module));
			}
			std::string key = original_key;
			std::ranges::replace(key, ' ', '_');
			tbl[addr_cache_group][key] = arr;
		}

		if (std::ofstream file(settings_name, std::ios::trunc); file.is_open()) {
			file << tbl;
		}
	}
} // namespace rivet_hook
