// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <ranges>

#include <wrl/client.h>
#include <dstorage.h>
#include <initguid.h>

#include "runtime.hpp"
#include "runtime_loader.hpp"
#include "settings.hpp"
#include "signature.hpp"

#include "MinHook.h"

DEFINE_GUID(IID_IDStorageQueue1, 0xdd2f482c, 0x5eff, 0x41e8, 0x9c, 0x9e, 0xd2, 0x37, 0x4b, 0x27, 0x81, 0x28);
DEFINE_GUID(IID_IDStorageFactory, 0x6924ea0c, 0xc3cd, 0x4826, 0xb1, 0x0a, 0xf6, 0x4f, 0x4e, 0xd9, 0x27, 0xc1);

namespace rivet_hook {
	constexpr uint64_t RIVET_SENTINEL = 0xffffffff'ffffff00;

	struct MemoryFile {
		const uint8_t *buffer = nullptr;
		HANDLE map = INVALID_HANDLE_VALUE;
		HANDLE file = INVALID_HANDLE_VALUE;
		size_t size = 0;
		AssetLanguage language = AssetLanguage::None;
		std::filesystem::path original_path;

		explicit MemoryFile(const std::filesystem::path &path): original_path(path) {
			file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE) {
				g_output << "[io] cannot open " << path.string() << " got " << GetLastError() << "\n";
				return;
			}

			GetFileSizeEx(file, reinterpret_cast<LARGE_INTEGER *>(&size));

			map = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
			if (map == INVALID_HANDLE_VALUE) {
				g_output << "[io] cannot map " << path.string() << " got " << GetLastError() << "\n";
				return;
			}

			buffer = static_cast<const uint8_t *>(MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0));
			if (buffer == nullptr) {
				g_output << "[io] cannot pin " << path.string() << " got " << GetLastError() << "\n";
				return;
			}
		}

		MemoryFile(const MemoryFile &) = delete;
		MemoryFile &
		operator=(const MemoryFile &) = delete;

		[[nodiscard]] auto
		valid() const -> bool {
			return buffer != nullptr && map != INVALID_HANDLE_VALUE && file != INVALID_HANDLE_VALUE && size > 0;
		}

		auto
		close() -> void {
			if (buffer != nullptr) {
				UnmapViewOfFile(buffer);
				buffer = nullptr;
			}

			if (map != INVALID_HANDLE_VALUE) {
				CloseHandle(map);
				map = INVALID_HANDLE_VALUE;
			}

			if (file != INVALID_HANDLE_VALUE) {
				CloseHandle(file);
				file = INVALID_HANDLE_VALUE;
			}
		}
	};

	bool runtime_loader_ready = false;
	using mod_file_list_t = std::unordered_map<AssetId, MemoryFile>;
	using mod_list_t = std::array<mod_file_list_t, static_cast<int32_t>(AssetType::Count)>;
	std::array<mod_list_t, static_cast<int32_t>(AssetLanguage::Count)> mod_files_combined = {};

	std::array known_important_assets = {
		0x8e7f2fafc675d9ef,
		0xa5fd9d73bb4f722e,
		0xb575e9facf1a38bc,
		0xabe81779b7c45edf,
		0x82ce4031e142c7f3,
		0x8323511e0074e322,
		0x98aa90ad5ea29cf5,
	};

	std::array<std::string_view, static_cast<int32_t>(AssetLanguage::Count)> rivet_lang_prefix = {
		"none", "us", "gb", "dk", "nl", "fi", "fr", "de", "it", "jp", "kr", "no", "pl", "pt", "ru", "es",
		"se", "br", "ar", "tr", "la", "cs", "ct", "fc", "cz", "hu", "el", "ro", "th", "vi", "id", "hr",
	};

	std::array<std::string_view, static_cast<int32_t>(AssetType::Count)> rivet_exts = {
		"",
		".stream",
		".unk2strm",
		".wem",
		".unk4strm",
		".animstrm",
		".unk6strm",
		".lgstream",
	};

	auto text_language = AssetLanguage::None;
	auto audio_language = AssetLanguage::None;

	create_asset_id_t game_create_asset_id = nullptr;
	is_valid_asset_t game_is_valid_asset = nullptr;
	is_valid_asset_t game_is_installed_asset = nullptr;
	open_file_t game_open_file = nullptr;
	read_file_t game_read_file = nullptr;
	close_file_t game_close_file = nullptr;
	decode_url_t game_decode_url = nullptr;
	mgr_load_asset_t game_mgr_load_asset = nullptr;
	sort_t game_sort = nullptr;
	mount_archive_t game_mount_archive = nullptr;
	commit_assets_t game_commit_assets = nullptr;
	alloc_asset_t game_alloc_asset = nullptr;
	resolve_asset_t game_resolve_asset = nullptr;
	set_language_t game_set_text_language = nullptr;
	set_language_t game_set_audio_language = nullptr;
	window_init_t game_window_init = nullptr;
	is_asset_valid_t game_is_asset_valid = nullptr;
	nextgen_load_data_t game_nextgen_load_data = nullptr;
	dstorage_get_factory_t game_dstorage_get_factory = nullptr;
	dstorage_enqueue_request_t game_dstorage_enqueue_request = nullptr;
	LPVOID dll_dstorage_get_factory = nullptr;

	create_asset_t *game_create_asset = nullptr;
	void *game_create_asset_data = nullptr;
	LoadOperation *game_load_ops = nullptr;
	SortFunc game_sort_op = {};
	bool *legacy_texture_loading = nullptr;
	bool *disable_directstorage = nullptr;

	Microsoft::WRL::ComPtr<IDStorageQueue1> dstorage_queue = nullptr;

	auto
	create_asset_id(AssetId *asset_id, const char *asset_name) -> AssetId * {
		const auto result = game_create_asset_id(asset_id, asset_name);

		if (asset_name && *asset_name && asset_id) {
			g_output << "[asset id] " << std::hex << *asset_id << " " << asset_name << "\n";
		}

		return result;
	}

	auto
	decode_url(const char *url, const unsigned int urlLen, char *decoded, unsigned int *decodedSize) -> void {
		if (url != nullptr) {
			g_output << "[cohtml] " << url << "\n";
		}

		game_decode_url(url, urlLen, decoded, decodedSize);
	}

	auto
	mgr_load_asset(const intptr_t self, const AssetId asset_id, const AssetId parent_asset_id, const char *asset_name, const intptr_t referencing_asset, const intptr_t unknown6,
				   const int32_t unknown7) -> intptr_t {
		g_output << "[load asset] " << std::hex << asset_id << " ";

		if (asset_name && *asset_name) {
			g_output << asset_name << " from ";
		} else {
			g_output << "(null) from ";
		}

		if (referencing_asset) {
			if (const auto upper_path = reinterpret_cast<const char **>(referencing_asset + 0x10); upper_path && *upper_path && **upper_path) {
				g_output << *upper_path;
			} else {
				g_output << "(null)";
			}
		} else {
			g_output << "(nowhere)";
		}

		g_output << "\n";

		return game_mgr_load_asset(self, asset_id, parent_asset_id, asset_name, referencing_asset, unknown6, unknown7);
	}

	auto
	hook_cohtml() -> void {
		const HMODULE mod = GetModuleHandleA("cohtml.WindowsDesktop.dll");
		if (!mod) {
			g_output << "cannot hook cohtml, not loaded yet.\n";
			return;
		}

		const auto proc = reinterpret_cast<LPVOID>(GetProcAddress(mod, decode_url_string_name));
		if (!proc) {
			g_output << "cannot hook cohtml, export not found.\n";
			return;
		}

		create_hook("cohtml", proc, reinterpret_cast<LPVOID>(decode_url), reinterpret_cast<LPVOID *>(&game_decode_url));
	}

	auto
	find_mod_asset(const AssetId asset_id, AssetType type, AssetLanguage lang) -> MemoryFile * {
		if (lang >= AssetLanguage::Count || type >= AssetType::Count) {
			return nullptr;
		}

		auto &mod_list = mod_files_combined[static_cast<int32_t>(lang)][static_cast<int32_t>(type)];
		const auto &mod_index = mod_list.find(asset_id);

		if (mod_index == mod_list.end()) {
			return nullptr;
		}

		return &mod_index->second;
	}

	auto
	find_mod_asset(const AssetId asset_id, const AssetType type) -> MemoryFile * {
		auto *mod_file = find_mod_asset(asset_id, type, text_language);
		if (mod_file) {
			return mod_file;
		}

		mod_file = find_mod_asset(asset_id, type, audio_language);
		if (mod_file) {
			return mod_file;
		}

		mod_file = find_mod_asset(asset_id, type, AssetLanguage::None);
		if (mod_file) {
			return mod_file;
		}

		return nullptr;
	}

	auto
	has_mod_asset(const AssetId asset_id, AssetType type, AssetLanguage lang) -> bool {
		if (lang >= AssetLanguage::Count || type >= AssetType::Count) {
			return false;
		}

		const auto &mod_list = mod_files_combined[static_cast<int32_t>(lang)][static_cast<int32_t>(type)];
		return mod_list.contains(asset_id);
	}

	auto
	has_mod_asset(const AssetId asset_id, const AssetType type) -> bool {
		return has_mod_asset(asset_id, type, text_language) || has_mod_asset(asset_id, type, audio_language) || has_mod_asset(asset_id, type, AssetLanguage::None);
	}

	auto
	rivet_get_asset_lang(const std::string_view stem) -> AssetLanguage {
		const auto it = std::ranges::find(rivet_lang_prefix, stem);
		return static_cast<AssetLanguage>(it != rivet_lang_prefix.end() ? std::distance(rivet_lang_prefix.begin(), it) : 0);
	}

	auto
	rivet_get_asset_type(const std::string_view ext) -> AssetType {
		const auto it = std::ranges::find(rivet_exts, ext);
		return static_cast<AssetType>(it != rivet_exts.end() ? std::distance(rivet_exts.begin(), it) : 0);
	}

	auto
	populate_mod_asset(const std::filesystem::path &path, const std::string &game_path, AssetId asset_id, AssetType type, AssetLanguage lang) -> void {
		g_output << std::hex << "[loader] " << path.string() << " resolved to " << game_path << " with asset id " << asset_id << ", type " << static_cast<int32_t>(type) << ", language "
				 << static_cast<int32_t>(lang) << "\n";

		auto &mod_list = mod_files_combined[static_cast<int32_t>(lang)][static_cast<int32_t>(type)];

		if (const auto &mod_index = mod_list.find(asset_id); mod_index != mod_list.end()) {
			g_output << "[loader] " << path << " has a collision with " << mod_index->second.original_path << "; unloading old...\n";

			mod_index->second.close();
			mod_list.erase(asset_id);
		}

		auto [it, inserted] = mod_list.emplace(asset_id, path);
		if (!inserted) {
			g_output << "[loader] " << path << " did not insert?? \n";
			return;
		}

		auto &created = it->second;
		created.language = lang;

		if (!created.valid()) {
			g_output << "[loader] " << path << " failed to init\n";
			created.close();
			mod_list.erase(asset_id);
		}
	}

	auto
	load_mod_assets_rivet(const std::filesystem::path &base_dir) -> void {
		// localization/localization_us.localization -> localization/localization_all.localization (built, us)
		// sound/soundbank/us/init.bnk -> sound/soundbank/init.bnk (soundbank, us)
		// sound/wem/us/1.wem -> E000000000000001 (audio, us)
		// built/misc.texture.stream -> built/misc.texture (texture, none)
		// built/misc.texture -> built/misc.texture (built, none)
		for (const auto &entry : std::filesystem::recursive_directory_iterator(base_dir)) {
			if (!entry.is_regular_file()) {
				continue;
			}

			const auto &mod_path = entry.path();
			auto relative_path = std::filesystem::relative(mod_path, base_dir);

			auto language = AssetLanguage::None;
			const AssetType type = rivet_get_asset_type(relative_path.extension().string());

			if (relative_path.extension() == ".localization" && relative_path.filename() != "localization_all.localization") {
				std::string stem = relative_path.stem().string();

				if (const size_t underscore_pos = stem.find_last_of('_'); underscore_pos != std::string::npos) {
					language = rivet_get_asset_lang(std::string_view(stem).substr(underscore_pos + 1));
				}

				if (language == AssetLanguage::None) {
					continue; // malformed.
				}

				relative_path = relative_path.replace_filename("localization_all.localization");
			} else if (relative_path.has_parent_path()) {
				language = rivet_get_asset_lang(relative_path.parent_path().filename().string());
				if (language != AssetLanguage::None) {
					relative_path = relative_path.parent_path().parent_path() / relative_path.filename();
				}
			}

			AssetId asset_id = 0;
			if (type == AssetType::Audio) {
				try {
					asset_id = 0xE0000000'00000000 | std::stoul(relative_path.stem().string());
				} catch (const std::exception &e) {
					g_output << "could not parse asset id for path " << relative_path << ": " << e.what() << "\n";
					continue;
				}
			} else {
				if (auto first_dir = *relative_path.begin(); first_dir == "unknown") {
					try {
						asset_id = std::stoull(relative_path.stem().string(), nullptr, 16);
					} catch (const std::exception &e) {
						g_output << "could not parse asset id for path " << relative_path << ": " << e.what() << "\n";
						continue;
					}
				} else {
					if (type != AssetType::Built) {
						relative_path = relative_path.replace_extension("");
					}

					game_create_asset_id(&asset_id, relative_path.string().c_str());
				}
			}

			populate_mod_asset(mod_path, relative_path.string(), asset_id, type, language);
		}
	}

	auto
	load_mod_assets_overstrike(const std::filesystem::path &base_dir) -> void {
		// 0/... -> ... (built, none)
		// 1/... -> ... (texture, none)
		// 8/... -> ... (built, us)
		// 9/... -> ... (texture, us)
		for (const auto &entry : std::filesystem::directory_iterator(base_dir)) {
			if (!entry.is_directory()) {
				continue;
			}

			auto &entry_path = entry.path();

			uint32_t directory_id = 0;
			try {
				directory_id = std::stoul(entry_path.filename().string());
			} catch (const std::exception &e) {
				g_output << "could not parse group id for path " << entry_path << ": " << e.what() << "\n";
				continue;
			}

			if (directory_id > 0xff) {
				g_output << "group id for " << entry_path << " is malformed. skipping\n";
				continue;
			}

			const auto language = static_cast<AssetLanguage>(directory_id / 8);
			const auto type = static_cast<AssetType>(directory_id % 8);

			for (const auto &type_entry : std::filesystem::recursive_directory_iterator(entry_path)) {
				if (!type_entry.is_regular_file()) {
					continue;
				}

				const auto &mod_path = type_entry.path();
				auto relative_path = std::filesystem::relative(mod_path, entry_path);

				AssetId asset_id = 0;
				if (relative_path.extension() == "") {
					try {
						asset_id = std::stoull(relative_path.filename().string(), nullptr, 16);
					} catch (const std::exception &e) {
						g_output << "could not parse asset id for path " << relative_path << ": " << e.what() << "\n";
						continue;
					}
				} else {
					game_create_asset_id(&asset_id, relative_path.string().c_str());
				}

				populate_mod_asset(mod_path, relative_path.string(), asset_id, type, language);
			}
		}
	}

	auto
	load_mod_assets() -> void {
		const auto cwd = std::filesystem::current_path();

		for (const auto &entry : g_settings.asset_paths) {
			auto path = std::filesystem::path(entry);

			if (!path.is_absolute()) {
				path = cwd / path;
			}

			if (!std::filesystem::is_directory(path)) {
				g_output << "[loader] mod path " << entry << " does not exist! skipping.\n";
				continue;
			}

			if (std::filesystem::exists(path / "info.json")) {
				g_output << "[loader] mod path " << entry << " is overstrike format.\n";
				load_mod_assets_overstrike(path);
			} else {
				g_output << "[loader] mod path " << entry << " is rivet format.\n";
				load_mod_assets_rivet(path);
			}
		}
		g_output.flush();
	}

	auto
	has_mod_asset(const AssetId asset_id) -> bool {
		for (auto type = 0; type < static_cast<int32_t>(AssetType::Count); type++) {
			if (has_mod_asset(asset_id, static_cast<AssetType>(type))) {
				return true;
			}
		}

		return false;
	}

	auto
	open_file(const intptr_t self, AssetFile *file, const AssetId asset_id, AssetType type, const int32_t platform, const uint8_t manager_id) -> void {
		if (g_settings.log_loose_io) {
			g_output << "[loose][open ] " << std::hex << asset_id << " type: " << static_cast<int32_t>(type) << " manager: " << static_cast<uint32_t>(manager_id) << " status: " << file->status
					 << " padding: " << file->padding << " data: " << file->data << " asset_id: " << file->asset_id << "\n";
		}

		if (type < AssetType::Count) {
			if (has_mod_asset(asset_id, type)) {
				if (g_settings.log_mod_access) {
					g_output << "[loose][open ] " << std::hex << asset_id << " is modded\n";
					g_output.flush();
				}

				file->status = 2;
				file->padding = 0;
				file->data = RIVET_SENTINEL | static_cast<uint8_t>(static_cast<int32_t>(type));
				file->asset_id = asset_id;
				return;
			}
		}

		g_output.flush();
		game_open_file(self, file, asset_id, type, platform, manager_id);
	}

	auto
	read_file(const intptr_t self, AssetFile *file, char *buffer, const size_t offset, const size_t size, const int32_t priority, const int32_t unknown2) -> bool {
		if (g_settings.log_loose_io) {
			g_output << "[loose][read ] offset: " << std::hex << offset << " size: " << size << " status: " << file->status << " padding: " << file->padding << " data: " << file->data
					 << " asset_id: " << file->asset_id << "\n";
			g_output.flush();
		}

		if (const auto type = static_cast<AssetType>(file->data & 0xFF); (file->data & RIVET_SENTINEL) == RIVET_SENTINEL && type < AssetType::Count) {
			file->status = 0x8000000a;

			if (const auto *mod_file = find_mod_asset(file->asset_id, type); mod_file != nullptr) {
				if (offset + size > mod_file->size) {
					return false;
				}

				std::copy_n(mod_file->buffer + offset, size, buffer);

				file->status = 3;
				return true;
			}

			return false;
		}

		return game_read_file(self, file, buffer, offset, size, priority, unknown2);
	}

	auto
	close_file(const intptr_t self, AssetFile *file) -> void {
		if (g_settings.log_loose_io) {
			g_output << "[loose][close] status: " << file->status << " padding: " << file->padding << " data: " << file->data << " asset_id: " << file->asset_id << "\n";
			g_output.flush();
		}

		if (const auto type = static_cast<AssetType>(file->data & 0xFF); (file->data & RIVET_SENTINEL) == RIVET_SENTINEL && type < AssetType::Count) {
			file->data = 0;
		} else {
			game_close_file(self, file);
		}
	}

	auto
	reimpl_load_ops(ArchiveFileSystem *self, const AssetId *assetIds, const LoadMetadata *metadata, const int32_t assetCount) -> int64_t {
		if (assetCount <= 0) {
			return 0;
		}

		int32_t loadIndex = 0;
		for (int32_t i = 0; i < assetCount; ++i) {
			LoadMetadata meta = metadata[i];
			uint64_t assetId = assetIds[i];

			if (g_settings.log_asset_opens) {
				g_output << "[built] " << std::hex << assetId << " type: " << static_cast<uint32_t>(meta.type) << "\n";
			}

			const MemoryFile *mod_file = nullptr;
			if (meta.type == 0xE /* soundbank */) {
				mod_file = find_mod_asset(assetId, AssetType::Built, audio_language);
			}

			if (mod_file == nullptr) {
				mod_file = find_mod_asset(assetId, AssetType::Built);
			}

			if (mod_file != nullptr && mod_file->valid()) {
				if (g_settings.log_mod_access) {
					g_output << "[built] " << std::hex << assetId << " is modded\n";
				}

				if (g_settings.log_mod_access && g_settings.log_mod_state) {
					g_output << "[built] " << std::hex << assetId << " create header\n";
				}

				AssetHeader *header = game_alloc_asset(0, 1, assetId, &meta, static_cast<uint8_t>(mod_file->language));

				if (g_settings.log_mod_access && g_settings.log_mod_state) {
					g_output << "[built] " << std::hex << assetId << " header created\n";
				}

				if (header) {
					if (g_settings.log_mod_access && g_settings.log_mod_state) {
						g_output << "[built] " << std::hex << assetId << " check valid, ptr " << reinterpret_cast<intptr_t>(mod_file->buffer) << "\n";
					}

					const auto magic = *reinterpret_cast<const uint32_t *>(mod_file->buffer);
					if (g_settings.log_mod_access && g_settings.log_mod_state) {
						g_output << "[built] " << std::hex << assetId << " magic " << magic << "\n";
					}

					if (mod_file->size <= 0x24 || !game_is_asset_valid(magic, meta.type, assetId)) {
						if (g_settings.log_mod_access && g_settings.log_mod_state) {
							g_output << "[built] " << std::hex << assetId << " not valid\n";
						}

						header->status = 7;
						goto commit;
					}

					if (g_settings.log_mod_access && g_settings.log_mod_state) {
						g_output << "[built] " << std::hex << assetId << " valid, create\n";
					}

					if ((*game_create_asset)(header, mod_file->buffer, game_create_asset_data)) {
						if (g_settings.log_mod_access && g_settings.log_mod_state) {
							g_output << "[built] " << std::hex << assetId << " created\n";
						}

						intptr_t offset = 0x24;
						for (int32_t j = 0; j < header->dataRangeCount; ++j) {
							if (static_cast<size_t>(offset + header->dataRanges[j].size) > mod_file->size) {
								if (g_settings.log_mod_access && g_settings.log_mod_state) {
									g_output << "[built] " << std::hex << assetId << " out of bounds\n";
								}

								header->status = 6;
								break;
							}

							if (g_settings.log_mod_access && g_settings.log_mod_state) {
								g_output << "[built] " << std::hex << assetId << " copy\n";
							}

							std::copy_n(mod_file->buffer + offset, header->dataRanges[j].size, header->dataRanges[j].buffer);
							offset += header->dataRanges[j].size;
						}

						if (g_settings.log_mod_access && g_settings.log_mod_state) {
							g_output << "[built] " << std::hex << assetId << " done\n";
						}

						header->status = 0;
						goto commit;
					}

					if (g_settings.log_mod_access && g_settings.log_mod_state) {
						g_output << "[built] " << std::hex << assetId << " cant create\n";
					}
					header->status = 4;

				commit:
					if (g_settings.log_mod_access && g_settings.log_mod_state) {
						g_output << "[built] " << std::hex << assetId << " commit header\n";
					}

					game_commit_assets(1);
				}

				continue;
			}

			const FoundAsset *asset = nullptr;
			AssetLanguage selectedLanguage = audio_language;
			if (meta.type == 0xE /* soundbank */) {
				asset = game_resolve_asset(&self->toc, assetId, audio_language, AssetType::Built);
			}

			if (!asset) {
				asset = game_resolve_asset(&self->toc, assetId, text_language, AssetType::Built);
				selectedLanguage = text_language;
			}

			if (!asset) {
				asset = game_resolve_asset(&self->toc, assetId, audio_language, AssetType::Built);
				selectedLanguage = audio_language;
			}

			if (!asset) {
				asset = game_resolve_asset(&self->toc, assetId, AssetLanguage::None, AssetType::Built);
				selectedLanguage = AssetLanguage::None;
			}

			if (!asset || asset->header == -1) {
				// here in case of crash becasue i haven't seen this yet
				// there's 3 different ways it fails early prior to this so if it happens here something really bad happened

				g_output << "[built] invalid path " << std::hex << assetId << "\n";

				if (game_alloc_asset(0, 1, assetId, &meta, meta.language)) {
					game_commit_assets(1);
				}

				g_output << "[built] skipped " << std::hex << assetId << "\n";

				continue;
			}

			const ArchiveAsset archiveAsset = asset->asset;

			if (!self->mountedTable[archiveAsset.index]) {
				game_mount_archive(self, archiveAsset.index);
			}

			game_load_ops[loadIndex].index = i;
			game_load_ops[loadIndex].asset = archiveAsset;
			game_load_ops[loadIndex].size = asset->size;
			game_load_ops[loadIndex].header = asset->header;
			game_load_ops[loadIndex].language = static_cast<uint8_t>(selectedLanguage);
			game_load_ops[loadIndex].priority |= 1u;

			if (std::ranges::find(known_important_assets, assetId) != known_important_assets.end()) {
				game_load_ops[loadIndex].priority &= ~1u;
			}

			loadIndex += 1;
		}

		game_sort(reinterpret_cast<intptr_t>(game_load_ops), loadIndex, 0x18, game_sort_op);

		g_output.flush();
		return loadIndex;
	}

	auto
	is_valid_asset(ArchiveFileSystem *self, const AssetId asset_id) -> bool {
		return game_is_valid_asset(self, asset_id) || has_mod_asset(asset_id);
	}

	auto
	is_installed_asset(ArchiveFileSystem *self, const AssetId asset_id) -> bool {
		return game_is_installed_asset(self, asset_id) || has_mod_asset(asset_id);
	}

	auto
	set_text_language(const AssetLanguage lang) -> void {
		game_set_text_language(lang);
		text_language = lang;
	}

	auto
	set_audio_language(const AssetLanguage lang) -> void {
		game_set_audio_language(lang);
		audio_language = lang;
	}

	auto
	window_init(const intptr_t self) -> bool {
		const auto result = game_window_init(self);

		// NOTE: This bricks DirectStorage, need to find a workaround for "next gen" texture fencing.
		*legacy_texture_loading = true;
		*disable_directstorage = true;

		return result;
	}

	auto
	dstorage_get_factory(REFIID riid, void** ppv) -> HRESULT {
		const auto result = game_dstorage_get_factory(riid, ppv);

		if (memcmp(&riid, &IID_IDStorageFactory, sizeof(IID_IDStorageFactory)) == 0) {
			MH_DisableHook(dll_dstorage_get_factory);

			auto *factory = *reinterpret_cast<IDStorageFactory **>(ppv);
			using Microsoft::WRL::ComPtr;

			DSTORAGE_QUEUE_DESC queueSetup = {};
			queueSetup.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
			queueSetup.Priority = DSTORAGE_PRIORITY_NORMAL;
			queueSetup.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;
			queueSetup.Device = nullptr;

			if (FAILED(factory->CreateQueue(&queueSetup, IID_IDStorageQueue1, reinterpret_cast<void **>(dstorage_queue.GetAddressOf())))) {
				g_output << "[dstorage] could not create dstorage queue\n";
				g_output.flush();
			}
		}

		return result;
	}

	auto
	hook_dstorage_factory() -> void {
		g_output << "[dstorage] attempting to find factory ptr\n";
		g_output.flush();

		auto directStorageModule = GetModuleHandleA("dstorage.dll");
		if (!directStorageModule) {
			directStorageModule = LoadLibraryA("dstorage.dll");
		}

		if (!directStorageModule) {
			g_output << "[dstorage] dstorage.dll is not present\n";
			g_output.flush();
			return;
		}

		dll_dstorage_get_factory = reinterpret_cast<LPVOID>(GetProcAddress(directStorageModule, "DStorageGetFactory"));
		create_hook("dstorage get factory", dll_dstorage_get_factory, reinterpret_cast<LPVOID>(&dstorage_get_factory), reinterpret_cast<LPVOID *>(&game_dstorage_get_factory));
	}

	auto
	nextgen_load_data(void* asset, const int32_t lods) -> bool {
		/*
		auto mod_file = find_mod_asset(asset->asset_id, AssetType::Texture);
		if (!mod_file) {
			return game_NextGen_LoadData(asset, lods);
		}

		if (g_settings.log_mod_access) {
			g_output << "[loose][open ] " << std::hex << asset_id << " is modded\n";
			g_output.flush();
		}

		CriticalSectionGuard guard(ptr_TextureMutex);
		if(!guard.success) {
			return false;
		}

		HighMipData data;
		if(!game_InitHighMips(asset, &data, lods)) {
			return false;
		}

		game_CreateTextureResource(asset, &data);

		for (uint32_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex) {
			std::copy_n(...);
		}

		is data copied anywhere??
		*/
		return game_nextgen_load_data(asset, lods);
	}

	auto
	AssetLoader::init() -> void {
		if (runtime_loader_ready) {
			return;
		}
		runtime_loader_ready = true;

		if (const auto create_asset_id_ptrs = find_addresses("asset ids", g_game_module, CREATE_ASSET_ID_SIGNATURE); !create_asset_id_ptrs.empty()) {
			if (g_settings.log_asset_ids) {
				create_hook("asset ids", reinterpret_cast<LPVOID>(create_asset_id_ptrs[0]), reinterpret_cast<LPVOID>(&create_asset_id), reinterpret_cast<LPVOID *>(&game_create_asset_id));
			} else {
				game_create_asset_id = reinterpret_cast<create_asset_id_t>(create_asset_id_ptrs[0]);
			}
		}

		if (g_settings.log_paths) {
			create_hook("asset paths", g_game_module, LOAD_ASSET_SIGNATURE, reinterpret_cast<LPVOID>(&mgr_load_asset), reinterpret_cast<LPVOID *>(&game_mgr_load_asset));
		}

		if (g_settings.log_cohtml) {
			hook_cohtml();
		}

		if (!g_settings.enable_asset_loader) {
			return;
		}

		load_mod_assets();

		#define LOAD_FUNC_ADDRESS_RAW(var, name, ptr) \
		if (var = find_address(name, g_game_module, ptr); !var) { \
		g_output << "[loader] cannot initialize, " name " address is not found\n"; \
		return; \
		}

		#define LOAD_FUNC_ADDRESS(var, name, type, ptr) \
		if (var = reinterpret_cast<type>(find_address(name, g_game_module, ptr)); !var) { \
		g_output << "[loader] cannot initialize, " name " address is not found\n"; \
		return; \
		}

		#define LOAD_VAR_ADDRESS(var, name, type, ptr, addr) \
			if (var = reinterpret_cast<type>(load_rel_var(find_address(name, g_game_module, ptr), addr)); !var) { \
				g_output << "[loader] cannot initialize, " name " address is not found\n"; \
				return; \
			}

		const auto * archivefs_vtable = static_cast<intptr_t*>(load_rel_var(find_address("archivefs", g_game_module, ARCHIVEFS_VTABLE_SIGNATURE), ARCHIVEFS_VTABLE_ADDRESS));
		if (!archivefs_vtable) {
			g_output << "[loader] cannot initialize, archivefs address is not found\n";
			return;
		}

		// functions we need to call for reimpl_load_ops
		LOAD_FUNC_ADDRESS(game_resolve_asset, "resolve asset", resolve_asset_t, RESOLVE_ASSET_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_alloc_asset, "alloc asset", alloc_asset_t, ALLOC_ASSET_RCRA_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_commit_assets, "commit asset", commit_assets_t, COMMIT_ASSET_RCRA_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_is_asset_valid, "is asset header valid", is_asset_valid_t, IS_ASSET_HEADER_VALID_RCRA_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_sort, "sort", sort_t, SORT_SIGNATURE);

		LOAD_FUNC_ADDRESS_RAW(game_sort_op.func, "sort op", SORT_FUNC_RCRA_SIGNATURE);
		game_sort_op.target = 0;

		game_mount_archive = reinterpret_cast<mount_archive_t>(archivefs_vtable[ARCHIVEFS_VTABLE_MOUNT]);
		if (!game_mount_archive) {
			g_output << "[loader] cannot initialize, mount archive address is not found\n";
		}

		// vars we need to read/write to for reimpl_load_ops
		LOAD_VAR_ADDRESS(game_load_ops, "load operations", LoadOperation*, LOAD_OPS_SIGNATURE, LOAD_OPS_ADDRESS);

		// vars we need to call/read to for asset header creation
		LOAD_VAR_ADDRESS(game_create_asset, "create asset", create_asset_t*, CREATE_ASSET_RCRA_SIGNATURE, CREATE_ASSET_RCRA_ADDRESS);
		LOAD_VAR_ADDRESS(game_create_asset_data, "create asset data", void*, CREATE_ASSET_DATA_RCRA_SIGNATURE, CREATE_ASSET_DATA_RCRA_ADDRESS);

		// vars we need to overwrite to disable texture fencing
		LOAD_VAR_ADDRESS(disable_directstorage, "disable directstorage", bool*, DISABLE_DIRECTSTORAGE_RCRA_SIGNATURE, DISABLE_DIRECTSTORAGE_RCRA_ADDRESS);
		LOAD_VAR_ADDRESS(legacy_texture_loading, "legacy textures", bool*, LEGACY_TEXTURE_SIGNATURE, LEGACY_TEXTURE_ADDRESS);

		// language tracking
		LPVOID set_text_lang, set_audio_lang;
		LOAD_VAR_ADDRESS(set_text_lang, "set text lang", LPVOID, REL_SET_TEXT_AUDIO_LANGUAGE_SIGNATURE, REL_SET_TEXT_LANGUAGE_ADDRESS);
		LOAD_VAR_ADDRESS(set_audio_lang, "set audio lang", LPVOID, REL_SET_TEXT_AUDIO_LANGUAGE_SIGNATURE, REL_SET_AUDIO_LANGUAGE_ADDRESS);
		create_hook("set text lang", set_text_lang, reinterpret_cast<LPVOID>(&set_text_language), reinterpret_cast<LPVOID *>(&game_set_text_language));
		create_hook("set audio lang", set_audio_lang, reinterpret_cast<LPVOID>(&set_audio_language), reinterpret_cast<LPVOID *>(&game_set_audio_language));

		// asset io
		create_hook("preload load op", g_game_module, PRELOAD_LOAD_OP_RCRA_SIGNATURE, reinterpret_cast<LPVOID>(&reimpl_load_ops), nullptr);
		create_hook("is valid asset", g_game_module, IS_ASSET_VALID_RCRA_SIGNATURE, reinterpret_cast<LPVOID>(&is_valid_asset), reinterpret_cast<LPVOID *>(&game_is_valid_asset));
		create_hook("is installed asset", g_game_module, IS_INSTALLED_ASSET_SIGNATURE, reinterpret_cast<LPVOID>(&is_installed_asset), reinterpret_cast<LPVOID *>(&game_is_installed_asset));

		// loose io
		create_hook("open file", reinterpret_cast<LPVOID>(archivefs_vtable[ARCHIVEFS_VTABLE_OPENFILE]), reinterpret_cast<LPVOID>(&open_file), reinterpret_cast<LPVOID *>(&game_open_file));
		create_hook("read file", reinterpret_cast<LPVOID>(archivefs_vtable[ARCHIVEFS_VTABLE_READFILE]), reinterpret_cast<LPVOID>(&read_file), reinterpret_cast<LPVOID *>(&game_read_file));
		create_hook("close file", reinterpret_cast<LPVOID>(archivefs_vtable[ARCHIVEFS_VTABLE_CLOSEFILE]), reinterpret_cast<LPVOID>(&close_file), reinterpret_cast<LPVOID *>(&game_close_file));

		hook_dstorage_factory();
		// create_hook("nextgen load", reinterpret_cast<LPVOID>(0x14135fcd0), reinterpret_cast<LPVOID>(&nextgen_load_data), reinterpret_cast<LPVOID *>(&game_nextgen_load_data));

		// disable fencing
		// NOTE: This bricks DirectStorage, need to find a workaround for "next gen" texture fencing.
		if (g_settings.force_legacy_textures) {
			// needed to reset fencing a second time once the game starts.
			create_hook("window init", g_game_module, WINDOW_INIT_RCRA_SIGNATURE, reinterpret_cast<LPVOID>(&window_init), reinterpret_cast<LPVOID *>(&game_window_init));

			*legacy_texture_loading = true;
			*disable_directstorage = true;
		}

		#undef RVA
	}

	auto
	AssetLoader::fini() -> void {
		if (!runtime_loader_ready) {
			return;
		}
		runtime_loader_ready = false;

		for (auto &type_mod_list : mod_files_combined) {
			for (auto &mod_list : type_mod_list) {
				for (auto &value : mod_list | std::views::values) {
					g_output << "[loader] closing " << value.original_path.string() << "\n";
					value.close();
				}

				mod_list.clear();
			}
		}

		if (dstorage_queue) {
			dstorage_queue->Release();
			dstorage_queue = nullptr;
		}
	}
} // namespace rivet_hook
