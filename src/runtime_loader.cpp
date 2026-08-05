// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#include <initguid.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <ranges>
#include <utility>
#include <dstorage.h>
#include <wrl/client.h>

#ifdef __MINGW64__
#define RIVET_DEFINE_IID(name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
		DEFINE_GUID(IID_ ## name, l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8); \
		__CRT_UUID_DECL(name, l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)

RIVET_DEFINE_IID(IDStorageFile, 0x5de95e7b, 0x955a, 0x4868, 0xa7, 0x3c, 0x24, 0x3b, 0x29, 0xf4, 0xb8, 0xda)

#undef RIVET_DEFINE_IID
#endif

#include <MinHook.h>

#include "game/asset_pipeline.hpp"
#include "runtime.hpp"
#include "runtime_loader.hpp"

#include "settings.hpp"
#include "signature.hpp"

namespace rivet_hook {
	constexpr int64_t RIVET_SENTINEL = 0x7fffffff'ffffff00;

	using namespace game;

	struct MemoryFile {
		const uint8_t *buffer = nullptr;
		HANDLE map = INVALID_HANDLE_VALUE;
		HANDLE file = INVALID_HANDLE_VALUE;
		size_t size = 0;
		AssetLanguage language = AssetLanguage::None;
		std::filesystem::path original_path;
		Microsoft::WRL::ComPtr<IDStorageFile> dstorageFile = nullptr;

		explicit MemoryFile(std::filesystem::path path): original_path(std::move(path)) {
			open();
		}

		MemoryFile(const MemoryFile &) = delete;
		MemoryFile &
		operator=(const MemoryFile &) = delete;

		auto
		open() -> void {
			if (valid()) {
				return;
			}

			file = CreateFileW(original_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE) {
				g_output << "[io] cannot open " << original_path.string() << " got " << GetLastError() << "\n";
				close();
				return;
			}

			GetFileSizeEx(file, reinterpret_cast<LARGE_INTEGER *>(&size));

			map = CreateFileMapping(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
			if (map == INVALID_HANDLE_VALUE) {
				g_output << "[io] cannot map " << original_path.string() << " got " << GetLastError() << "\n";
				close();
				return;
			}

			buffer = static_cast<const uint8_t *>(MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0));
			if (buffer == nullptr) {
				g_output << "[io] cannot pin " << original_path.string() << " got " << GetLastError() << "\n";
				close();
			}
		}

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

			dstorageFile.Reset();
		}

		auto
		get_dstorage(IDStorageFactory * factory) -> IDStorageFile * {
			if (!valid()) {
				return nullptr;
			}

			if (!factory) {
				g_output << "[dstorage] cannot map " << original_path << " to dstorage, no factory\n";
				g_output.flush();
				return nullptr;
			}

			if (!dstorageFile) {
				if (FAILED(factory->OpenFile(original_path.wstring().c_str(), IID_PPV_ARGS(&dstorageFile)))) {
					g_output << "[dstorage] cannot map " << original_path << " to dstorage\n";
					g_output.flush();
					return nullptr;
				}
			}

			return dstorageFile.Get();
		}
	};

	struct CriticalSectionGuard {
		LPCRITICAL_SECTION section;
		bool success;

		explicit CriticalSectionGuard(const LPCRITICAL_SECTION section) : section(section) {
			success = TryEnterCriticalSection(section) != 0;
		}

		~CriticalSectionGuard() {
			if (!success) {
				return;
			}

			LeaveCriticalSection(section);
		}

		CriticalSectionGuard(const CriticalSectionGuard&) = delete;
		CriticalSectionGuard& operator=(const CriticalSectionGuard&) = delete;
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

	std::array<std::string_view, static_cast<int32_t>(AssetLanguage::Count)> rivet_lang_prefix = {{
		"none", "us", "gb", "dk", "nl", "fi", "fr", "de", "it", "jp", "kr", "no", "pl", "pt", "ru", "es",
		"se", "br", "ar", "tr", "la", "cs", "ct", "fc", "cz", "hu", "el", "ro", "th", "vi", "id", "hr",
	}};

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

	create_asset_id_t game_create_asset_id = nullptr; // CREATE_ASSET_ID_SIGNATURE
	is_valid_asset_t game_is_valid_asset = nullptr; // IS_ASSET_VALID_RCRA_SIGNATURE
	is_valid_asset_t game_is_installed_asset = nullptr; // IS_INSTALLED_ASSET_SIGNATURE
	open_file_t game_open_file = nullptr; // ARCHIVEFS_VTABLE_OPENFILE
	read_file_t game_read_file = nullptr; // ARCHIVEFS_VTABLE_READFILE
	close_file_t game_close_file = nullptr; // ARCHIVEFS_VTABLE_CLOSEFILE
	resolve_handle_t game_resolve_handle = nullptr; // ARCHIVEFS_VTABLE_RESOLVEHANDLE
	set_file_status_t game_set_file_status = nullptr; // SET_FILE_STATUS_SIGNATURE
	decode_url_t game_decode_url = nullptr; // cohtml.WindowsDesktop.dll::?DecodeURLString@Library@cohtml@@SAXPEBDIPEADPEAI@Z
	mgr_load_asset_t game_mgr_load_asset = nullptr; // LOAD_ASSET_SIGNATURE
	sort_t game_sort = nullptr; // SORT_SIGNATURE
	mount_archive_t game_mount_archive = nullptr; // ARCHIVEFS_VTABLE_MOUNT
	commit_assets_t game_commit_assets = nullptr; // COMMIT_ASSET_RCRA_SIGNATURE
	alloc_asset_t game_alloc_asset = nullptr; // ALLOC_ASSET_RCRA_SIGNATURE
	resolve_asset_t game_resolve_asset = nullptr; // RESOLVE_ASSET_SIGNATURE
	set_language_t game_set_text_language = nullptr; // REL_SET_TEXT_AUDIO_LANGUAGE_SIGNATURE
	set_language_t game_set_audio_language = nullptr; // REL_SET_TEXT_AUDIO_LANGUAGE_SIGNATURE
	window_init_t game_window_init = nullptr; // WINDOW_INIT_RCRA_SIGNATURE
	is_asset_valid_t game_is_asset_valid = nullptr; // IS_ASSET_HEADER_VALID_RCRA_SIGNATURE

	nextgen_load_data_t game_nextgen_load_data = nullptr; // TODO!!
	init_mips_t game_init_mips = nullptr; // TODO!!
	get_storage_link_t game_get_storage_link = nullptr; // TODO!!
	create_texture_resource_t game_create_texture_resource = nullptr; // TODO!!
	dstorage_flush_queue_t game_dstorage_flush_queue = nullptr; // TODO!!
	dstorage_create_context_t game_dstorage_create_context = nullptr; // TODO!!
	dstorage_init_t game_dstorage_init = nullptr; // dstorage.dll@DStorageGetFactory

	create_asset_t *game_create_asset = nullptr; // CREATE_ASSET_RCRA_SIGNATURE
	void *game_create_asset_data = nullptr; // CREATE_ASSET_DATA_RCRA_SIGNATURE
	LoadOperation *game_load_ops = nullptr; // LOAD_OPS_SIGNATURE
	SortFunc game_sort_op = {}; // SORT_FUNC_RCRA_SIGNATURE
	bool *legacy_texture_loading = nullptr; // LEGACY_TEXTURE_SIGNATURE
	bool *disable_directstorage = nullptr; // DISABLE_DIRECTSTORAGE_RCRA_SIGNATURE

	IDStorageFactory* dstorage_factory = nullptr; // dstorage.dll@DStorageGetFactory
	NxDStorageWorkerContext* dstorage_context_texture = nullptr;
	NxDStorageWorkerContext* dstorage_context_bulk = nullptr; // never really used?
	LPCRITICAL_SECTION texture_lock = nullptr; // TODO!!

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
		LPVOID proc;
		if (const auto status = MH_CreateHookApiEx(L"cohtml.WindowsDesktop.dll", decode_url_string_name, reinterpret_cast<LPVOID>(decode_url), reinterpret_cast<LPVOID *>(&game_decode_url), &proc);
			status != MH_OK) {
			g_output << "[cohtml] cannot hook cohtml: " << MH_StatusToString(status) << "\n";
			return;
		}

		MH_EnableHook(proc);
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

		for (const auto &entry : g_settings.assets.paths) {
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
		if (g_settings.log.loose_io) {
			g_output << "[loose][open ] " << std::hex << asset_id << " type: " << static_cast<int32_t>(type) << " manager: " << static_cast<uint32_t>(manager_id) << " status: " << static_cast<uint32_t>(file->status)
					 << " padding: " << file->padding << " data: " << file->data << " asset_id: " << file->asset_id << "\n";
			g_output.flush();
		}

		if (type < AssetType::Count) {
			if (const auto *mod_file = find_mod_asset(asset_id, type); mod_file != nullptr) {
				if (g_settings.assets.log) {
					g_output << "[loose][open ] " << std::hex << asset_id << " type: " << static_cast<int32_t>(type) << " manager: " << static_cast<uint32_t>(manager_id) << " status: " << static_cast<uint32_t>(file->status)
							 << " padding: " << file->padding << " data: " << file->data << " asset_id: " << file->asset_id << "\n";
					g_output.flush();
				}

				file->status = AssetFileStatus::OpenComplete;
				file->padding = 0;
				file->data = RIVET_SENTINEL | static_cast<uint8_t>(static_cast<int32_t>(type));
				file->asset_id = asset_id;
				file->size = mod_file->size;
				return;
			}
		}

		game_open_file(self, file, asset_id, type, platform, manager_id);
	}

	auto
	resolve_handle(const intptr_t self, const AssetId asset_id, AssetType type, const int32_t platform, const uint8_t manager_id) -> int64_t {
		if (g_settings.log.loose_io) {
			g_output << "[loose][reslv] " << std::hex << asset_id << " type: " << static_cast<int32_t>(type) << " manager: " << static_cast<uint32_t>(manager_id) << "\n";
			g_output.flush();
		}

		if (type < AssetType::Count) {
			if (has_mod_asset(asset_id, type)) {
				if (g_settings.assets.log) {
					g_output << "[loose][reslv] " << std::hex << asset_id << " is modded\n";
					g_output.flush();
				}

				return static_cast<int64_t>(asset_id & INT64_MAX);
			}
		}

		return game_resolve_handle(self, asset_id, type, platform, manager_id);
	}

	auto
	read_file(const intptr_t self, AssetFile *file, char *buffer, const size_t offset, const size_t size, const int32_t priority, const int32_t unknown2) -> bool {
		if (g_settings.log.loose_io) {
			g_output << "[loose][read ] offset: " << std::hex << offset << " size: " << size << " status: " << static_cast<uint32_t>(file->status) << " padding: " << file->padding << " data: " << file->data
					 << " asset_id: " << file->asset_id << "\n";
			g_output.flush();
		}

		if (const auto type = static_cast<AssetType>(file->data & 0xFF); (file->data & RIVET_SENTINEL) == RIVET_SENTINEL && type < AssetType::Count) {
			if (g_settings.assets.log) {
				g_output << "[loose][read ] offset: " << std::hex << offset << " size: " << size << " status: " << static_cast<uint32_t>(file->status) << " padding: " << file->padding << " data: " << file->data
						 << " asset_id: " << file->asset_id << "\n";
				g_output.flush();
			}

			if (const auto *mod_file = find_mod_asset(file->asset_id, type); mod_file != nullptr) {
				if (offset + size > mod_file->size) {
					game_set_file_status(file, AssetFileStatus::OutOfBounds);
					return false;
				}

				std::copy_n(mod_file->buffer + offset, size, buffer);

				game_set_file_status(file, AssetFileStatus::ReadComplete);
				return true;
			}

			game_set_file_status(file, AssetFileStatus::ReadFailed);
			if (g_settings.assets.log) {
				g_output << "[loose][read ] trying to read something that does not exist.\n";
				g_output.flush();
			}

			return false;
		}

		return game_read_file(self, file, buffer, offset, size, priority, unknown2);
	}

	auto
	close_file(const intptr_t self, AssetFile *file) -> void {
		auto has_mod = false;
		if (const auto type = static_cast<AssetType>(file->data & 0xFF); (file->data & RIVET_SENTINEL) == RIVET_SENTINEL && type < AssetType::Count) {
			game_set_file_status(file, AssetFileStatus::Closed);
			file->data = 0;
			has_mod = g_settings.assets.log;
		} else {
			game_close_file(self, file);
		}

		if (g_settings.log.loose_io || has_mod) {
			g_output << "[loose][close] status: " << static_cast<uint32_t>(file->status) << " padding: " << file->padding << " data: " << file->data << " asset_id: " << file->asset_id << "\n";
			g_output.flush();
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

			if (g_settings.log.asset_io) {
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
				if (g_settings.assets.log) {
					g_output << "[built] " << std::hex << assetId << " is modded\n";
				}

				if (g_settings.assets.log && g_settings.assets.verbose) {
					g_output << "[built] " << std::hex << assetId << " create header\n";
				}

				AssetHeader *header = game_alloc_asset(0, 1, assetId, &meta, static_cast<uint8_t>(mod_file->language));

				if (g_settings.assets.log && g_settings.assets.verbose) {
					g_output << "[built] " << std::hex << assetId << " header created\n";
				}

				if (header) {
					if (g_settings.assets.log && g_settings.assets.verbose) {
						g_output << "[built] " << std::hex << assetId << " check valid, ptr " << reinterpret_cast<intptr_t>(mod_file->buffer) << "\n";
					}

					const auto magic = *reinterpret_cast<const uint32_t *>(mod_file->buffer);
					if (g_settings.assets.log && g_settings.assets.verbose) {
						g_output << "[built] " << std::hex << assetId << " magic " << magic << "\n";
					}

					if (mod_file->size <= 0x24 || !game_is_asset_valid(magic, meta.type, assetId)) {
						if (g_settings.assets.log && g_settings.assets.verbose) {
							g_output << "[built] " << std::hex << assetId << " not valid\n";
						}

						header->status = 7;
						goto commit;
					}

					if (g_settings.assets.log && g_settings.assets.verbose) {
						g_output << "[built] " << std::hex << assetId << " valid, create\n";
					}

					if ((*game_create_asset)(header, mod_file->buffer, game_create_asset_data)) {
						if (g_settings.assets.log && g_settings.assets.verbose) {
							g_output << "[built] " << std::hex << assetId << " created\n";
						}

						intptr_t offset = 0x24;
						for (int32_t j = 0; j < header->dataRangeCount; ++j) {
							if (static_cast<size_t>(offset + header->dataRanges[j].size) > mod_file->size) {
								if (g_settings.assets.log && g_settings.assets.verbose) {
									g_output << "[built] " << std::hex << assetId << " out of bounds\n";
								}

								header->status = 6;
								break;
							}

							if (g_settings.assets.log && g_settings.assets.verbose) {
								g_output << "[built] " << std::hex << assetId << " copy\n";
							}

							std::copy_n(mod_file->buffer + offset, header->dataRanges[j].size, header->dataRanges[j].buffer);
							offset += header->dataRanges[j].size;
						}

						if (g_settings.assets.log && g_settings.assets.verbose) {
							g_output << "[built] " << std::hex << assetId << " done\n";
						}

						header->status = 0;
						goto commit;
					}

					if (g_settings.assets.log && g_settings.assets.verbose) {
						g_output << "[built] " << std::hex << assetId << " cant create\n";
					}
					header->status = 4;

				commit:
					if (g_settings.assets.log && g_settings.assets.verbose) {
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

	auto WINAPI
	dstorage_init(REFIID riid, _COM_Outptr_ void** ppv) -> HRESULT {
		const auto result = game_dstorage_init(riid, ppv);

		if (dstorage_factory != nullptr) {
			g_output << "[dstorage] factory is being remade!\n";
			g_output.flush();
		} else {
			dstorage_factory = reinterpret_cast<IDStorageFactory *>(ppv);
			g_output << "[dstorage] factory found\n";
			g_output.flush();
		}

		return result;
	}

	auto
	dstorage_create_context(NxDStorageWorkerContext* context, void* callback, const int bufferSize, const char* name) {
		const auto result = game_dstorage_create_context(context, callback, bufferSize, name);

		if (name && name[0]) {
			if (strcmp(name, "Texture") == 0) {
				dstorage_context_texture = result;
			} else if (name && name[0] && strcmp(name, "Bulk") == 0) {
				dstorage_context_bulk = result;
			} else {
				g_output << "[nxstorage] unknown worker " << name << "\n";
			}
		}

		return result;
	}

	auto
	nextgen_load_data_tex(TextureAsset* asset, const int32_t levelOfDetail) -> bool {
		auto mod_file = find_mod_asset(asset->base.assetId, AssetType::TextureStream);
		if (!mod_file) {
			return game_nextgen_load_data(asset, levelOfDetail);
		}

		if (dstorage_context_texture == nullptr || texture_lock == nullptr) {
			g_output << "[nxtgn] " << std::hex << asset->base.assetId << " is modded but dstorage has not initialized?\n";
			g_output.flush();
			return game_nextgen_load_data(asset, levelOfDetail);
		}

		if (g_settings.assets.log || true) {
			g_output << "[nxtgn] " << std::hex << asset->base.assetId << " is modded\n";
			g_output.flush();
		}

		HighMipData data {};
		{
			// ReSharper disable once CppTooWideScopeInitStatement
			CriticalSectionGuard guard(texture_lock);

			if(!guard.success) {
				g_output << "[nxtgn] " << std::hex << asset->base.assetId << " cannot lock texture mutex\n";
				g_output.flush();
				return false;
			}

			if(!game_init_mips(asset, &data, levelOfDetail)) {
				return false;
			}

			game_create_texture_resource(asset, &data);
		}

		auto desc = *data.desc->resource12;
		auto mipLevels = data.mipLevels;
		auto width = data.width;
		auto height = data.height;
		auto totalSize = 0ul;

		for (uint32_t rangeIndex = 0; rangeIndex < data.numRanges; ++rangeIndex) {
			const uint32_t mip = rangeIndex % mipLevels;
			const uint32_t slice = rangeIndex / mipLevels;
			const uint64_t offset = data.fileRanges[rangeIndex].start;
			const uint64_t size = data.memRanges[rangeIndex].size;
			uint32_t mipWidth = width >> mip;
			uint32_t mipHeight = height >> mip;
			if (mipWidth < 1) {
				mipWidth = 1;
			}

			if (mipHeight < 1) {
				mipHeight = 1;
			}

			{
				// ReSharper disable once CppTooWideScopeInitStatement
				CriticalSectionGuard guard(&dstorage_context_texture->lock);
				if(!guard.success) {
					g_output << "[tex_nextgen] " << std::hex << asset->base.assetId << " cannot lock dstorage mutex\n";
					g_output.flush();
					return false;
				}

				auto *link = static_cast<NxDStorageWorkerEntry *>(game_get_storage_link(dstorage_context_texture, 0x78, 0x10));
				if (link == nullptr) {
					g_output << "[tex_nextgen] " << std::hex << asset->base.assetId << " cannot push dstorage state\n";
					g_output.flush();
					return false;
				}

				memset(link, 0, 0x78);
				link->resource = desc;
				link->mipIndex = -1;

				if (dstorage_context_texture->flushSignal) {
					link->flushSignal = INVALID_HANDLE_VALUE;
				} else if(link->next != link) {
					HANDLE signal = CreateEventW(nullptr, 1, 0, nullptr);
					dstorage_context_texture->flushSignal = signal;
					link->flushSignal = signal;
				}

				if (dstorage_context_texture->last) {
					dstorage_context_texture->last->next = link;
				}

				dstorage_context_texture->last = link;

				if (!dstorage_context_texture->first) {
					dstorage_context_texture->first = link;
				}
			}
			SetEvent(dstorage_context_texture->updateSignal);

			DSTORAGE_REQUEST req {};
			req.Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
			req.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION;
			if (auto file = mod_file->get_dstorage(dstorage_factory)) {
				req.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
				req.Source.File.Source = file;
				req.Source.File.Offset = offset;
				req.Source.File.Size = size;
			} else {
				req.Options.SourceType = DSTORAGE_REQUEST_SOURCE_MEMORY;
				req.Source.Memory.Source = mod_file->buffer + offset;
				req.Source.Memory.Size = size;
			}
			totalSize += size;
			req.Destination.Texture.Resource = desc;
			req.Destination.Texture.SubresourceIndex = slice;
			req.Destination.Texture.Region = { .left = 0, .top = 0, .front = 0, .right = mipWidth, .bottom = mipHeight, .back = 1 };
			req.CancellationTag = 0;
			req.UncompressedSize = size;
			dstorage_context_texture->queue->EnqueueRequest(&req);
		}

		// this triggers ID3DQueue->Submit, if the queue overflows the game will crash the gpu
		game_dstorage_flush_queue(dstorage_context_texture);

		asset->loadedLods &= 0xf0;
		asset->loadedLods |= levelOfDetail & 0xf;
		asset->resourceSize = totalSize;

		return true;
	}

	auto
	AssetLoader::init() -> void {
		if (runtime_loader_ready) {
			return;
		}
		runtime_loader_ready = true;

		if (const auto create_asset_id_ptrs = find_addresses(CREATE_ASSET_ID_SIGNATURE); !create_asset_id_ptrs.empty()) {
			if (g_settings.log.id) {
				create_hook("asset ids", reinterpret_cast<LPVOID>(create_asset_id_ptrs[0]), reinterpret_cast<LPVOID>(&create_asset_id), reinterpret_cast<LPVOID *>(&game_create_asset_id));
			} else {
				game_create_asset_id = reinterpret_cast<create_asset_id_t>(create_asset_id_ptrs[0]);
			}
		}

		if (g_settings.log.paths) {
			create_hook(LOAD_ASSET_SIGNATURE, reinterpret_cast<LPVOID>(&mgr_load_asset), reinterpret_cast<LPVOID *>(&game_mgr_load_asset));
		}

		if (g_settings.log.cohtml) {
			hook_cohtml();
		}

		if (!g_settings.assets.enabled) {
			return;
		}

		load_mod_assets();

		#define LOAD_FUNC_ADDRESS_RAW(var, name, sig) \
		if (var = find_address(sig); !var) { \
		g_output << "[loader] cannot initialize, " name " address is not found\n"; \
		return; \
		}

		#define LOAD_FUNC_ADDRESS(var, type, sig) \
		if (var = reinterpret_cast<type>(find_address(sig)); !var) { \
		g_output << "[loader] cannot initialize, " << sig.name << " address is not found\n"; \
		return; \
		}

		#define LOAD_VAR_ADDRESS(var, type, sig, addr) \
			if (var = reinterpret_cast<type>(load_rel_var(find_address(sig), addr)); !var) { \
				g_output << "[loader] cannot initialize, " << sig.name << " address is not found\n"; \
				return; \
			}

		const auto * archivefs_vtable = static_cast<intptr_t*>(load_rel_var(find_address(ARCHIVEFS_VTABLE_SIGNATURE), ARCHIVEFS_VTABLE_ADDRESS));
		if (!archivefs_vtable) {
			g_output << "[loader] cannot initialize, archivefs address is not found\n";
			return;
		}

		// functions we need to call for reimpl_load_ops
		LOAD_FUNC_ADDRESS(game_resolve_asset, resolve_asset_t, RESOLVE_ASSET_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_set_file_status, set_file_status_t, SET_FILE_STATUS_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_alloc_asset, alloc_asset_t, ALLOC_ASSET_RCRA_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_commit_assets, commit_assets_t, COMMIT_ASSET_RCRA_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_is_asset_valid, is_asset_valid_t, IS_ASSET_HEADER_VALID_RCRA_SIGNATURE);
		LOAD_FUNC_ADDRESS(game_sort, sort_t, SORT_SIGNATURE);

		LOAD_FUNC_ADDRESS_RAW(game_sort_op.func, "sort op", SORT_FUNC_RCRA_SIGNATURE);
		game_sort_op.target = 0;

		game_mount_archive = reinterpret_cast<mount_archive_t>(archivefs_vtable[ARCHIVEFS_VTABLE_MOUNT]);
		if (!game_mount_archive) {
			g_output << "[loader] cannot initialize, mount archive address is not found\n";
		}

		// vars we need to read/write to for reimpl_load_ops
		LOAD_VAR_ADDRESS(game_load_ops, LoadOperation*, LOAD_OPS_SIGNATURE, LOAD_OPS_ADDRESS);

		// vars we need to call/read to for asset header creation
		LOAD_VAR_ADDRESS(game_create_asset, create_asset_t*, CREATE_ASSET_RCRA_SIGNATURE, CREATE_ASSET_RCRA_ADDRESS);
		LOAD_VAR_ADDRESS(game_create_asset_data, void*, CREATE_ASSET_DATA_RCRA_SIGNATURE, CREATE_ASSET_DATA_RCRA_ADDRESS);

		// vars we need to overwrite to disable texture fencing
		LOAD_VAR_ADDRESS(disable_directstorage, bool*, DISABLE_DIRECTSTORAGE_RCRA_SIGNATURE, DISABLE_DIRECTSTORAGE_RCRA_ADDRESS);
		LOAD_VAR_ADDRESS(legacy_texture_loading, bool*, LEGACY_TEXTURE_SIGNATURE, LEGACY_TEXTURE_ADDRESS);

		// language tracking
		LPVOID set_text_lang, set_audio_lang;
		LOAD_VAR_ADDRESS(set_text_lang, LPVOID, REL_SET_TEXT_AUDIO_LANGUAGE_SIGNATURE, REL_SET_TEXT_LANGUAGE_ADDRESS);
		LOAD_VAR_ADDRESS(set_audio_lang, LPVOID, REL_SET_TEXT_AUDIO_LANGUAGE_SIGNATURE, REL_SET_AUDIO_LANGUAGE_ADDRESS);
		create_hook("SET_TEXT_LANGUAGE", set_text_lang, reinterpret_cast<LPVOID>(&set_text_language), reinterpret_cast<LPVOID *>(&game_set_text_language));
		create_hook("SET_AUDIO_LANGUAGE", set_audio_lang, reinterpret_cast<LPVOID>(&set_audio_language), reinterpret_cast<LPVOID *>(&game_set_audio_language));

		// asset io
		create_hook(PRELOAD_LOAD_OP_RCRA_SIGNATURE, reinterpret_cast<LPVOID>(&reimpl_load_ops), nullptr);
		create_hook(IS_ASSET_VALID_RCRA_SIGNATURE, reinterpret_cast<LPVOID>(&is_valid_asset), reinterpret_cast<LPVOID *>(&game_is_valid_asset));
		create_hook(IS_INSTALLED_ASSET_SIGNATURE, reinterpret_cast<LPVOID>(&is_installed_asset), reinterpret_cast<LPVOID *>(&game_is_installed_asset));

		// loose io
		create_hook("ARCHIVEFS_RESOLVE_HANDLE", reinterpret_cast<LPVOID>(archivefs_vtable[ARCHIVEFS_VTABLE_RESOLVEHANDLE]), reinterpret_cast<LPVOID>(&resolve_handle), reinterpret_cast<LPVOID *>(&game_resolve_handle));
		create_hook("ARCHIVEFS_OPEN_FILE", reinterpret_cast<LPVOID>(archivefs_vtable[ARCHIVEFS_VTABLE_OPENFILE]), reinterpret_cast<LPVOID>(&open_file), reinterpret_cast<LPVOID *>(&game_open_file));
		create_hook("ARCHIVEFS_READ_FILE", reinterpret_cast<LPVOID>(archivefs_vtable[ARCHIVEFS_VTABLE_READFILE]), reinterpret_cast<LPVOID>(&read_file), reinterpret_cast<LPVOID *>(&game_read_file));
		create_hook("ARCHIVEFS_CLOSE_FILE", reinterpret_cast<LPVOID>(archivefs_vtable[ARCHIVEFS_VTABLE_CLOSEFILE]), reinterpret_cast<LPVOID>(&close_file), reinterpret_cast<LPVOID *>(&game_close_file));

		LPVOID dstorageProc;
		if (const auto status = MH_CreateHookApiEx(L"dstorage.dll", "DStorageGetFactory", reinterpret_cast<LPVOID>(dstorage_init), reinterpret_cast<LPVOID *>(&game_dstorage_init), &dstorageProc);
			status != MH_OK) {
			g_output << "[loaded] cannot hook dstorage: " << MH_StatusToString(status) << "\n";
			g_settings.assets.disable_dstorage = true;
		} else {
			MH_EnableHook(dstorageProc);
		}

		if (g_settings.assets.disable_dstorage) {
			// needed to reset a second time once the game starts.
			create_hook(WINDOW_INIT_RCRA_SIGNATURE, reinterpret_cast<LPVOID>(&window_init), reinterpret_cast<LPVOID *>(&game_window_init));

			*legacy_texture_loading = true;
			*disable_directstorage = true;
		}
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
	}
} // namespace rivet_hook
