// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <ostream>
#include <thread>

#include "ddl.hpp"
#include "runtime.hpp"
#include "runtime_loader.hpp"
#include "settings.hpp"
#include "signature.hpp"
#include "signature_engine.hpp"

#include <MinHook.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-cast"

namespace {
	HMODULE g_renderdoc = nullptr;
	bool g_minhook_initialized = false;
	std::thread g_ddl_dump_thread;
} // namespace

namespace rivet_hook {
	std::ofstream g_output;
	Settings g_settings;
	HMODULE g_game_module = nullptr;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-bounds-pointer-arithmetic"
	using context_log_t = const char *(*) (const char *, const char *);
	context_log_t fwd_context_log = nullptr;
	std::string last_context;
	std::string last_message;

	auto
	find_addresses(const std::string &name, const HMODULE game, const hex_signature &signature) -> std::vector<intptr_t> {
		std::vector<intptr_t> pointers;
		if (!g_settings.addresses.contains(name)) {
			if (g_settings.log_hook_state) {
				g_output << "[rivet] searching for " << name << " pointers\n";
			}
			pointers = scan(game, signature);
			g_settings.addresses.emplace(name, pointers);
		} else {
			pointers = g_settings.addresses[name];
		}

		if (pointers.empty()) {
			g_output << "[rivet] could not find " << name << " pointer, aborting\n";
			return {};
		}

		if (g_settings.log_hook_state) {
			g_output << "[rivet] found " << pointers.size() << " " << name << " pointers" << std::dec << "\n";
		}

		return pointers;
	}

	auto
	find_address(const std::string &name, const HMODULE game, const hex_signature &signature, const size_t limit, const int select) -> intptr_t {
		const auto pointers = find_addresses(name, game, signature);

		if (pointers.empty()) {
			return 0;
		}

		if (pointers.size() > limit) {
			g_output << "[rivet] found " << pointers.size() << " " << name << " pointers, too many. aborting\n";
			return 0;
		}

		const auto pointer = pointers[select];

		if (g_settings.log_hook_state) {
			g_output << "[rivet] found " << name << " pointer at " << std::hex << pointer << std::dec << "\n";
		}

		return pointer;
	}

	auto
	load_rel_var(const intptr_t ptr, const int rel_address) -> void * {
		if (ptr == 0) {
			return nullptr;
		}

		const auto rip = reinterpret_cast<uint8_t*>(ptr) + rel_address + REL_ADDRESS_SIZE;
		const auto target = *reinterpret_cast<uint32_t *>(ptr + rel_address);
		return rip + target;
	}

	auto
	create_hook(const std::string &name, LPVOID pointer, LPVOID detour, LPVOID *original) -> void {
		if (!g_minhook_initialized) {
			if (MH_Initialize() != MH_OK) {
				g_output << "[rivet] failed to initialize minhook\n";
				return;
			}
			g_minhook_initialized = true;
		}

		if (MH_CreateHook(pointer, detour, original) != MH_OK) {
			g_output << "[rivet] failed to create " << name << " hook\n";
			return;
		}

		if (MH_EnableHook(pointer) != MH_OK) {
			g_output << "[rivet] failed to enable " << name << " hook\n";
			return;
		}

		if (g_settings.log_hook_state) {
			g_output << "[rivet] created " << name << " hook\n";
		}
	}

	auto
	create_hook(const std::string &name, const HMODULE game, const hex_signature &signature, LPVOID detour, LPVOID *original, const size_t limit, const int select) -> void {
		const auto pointer = find_address(name, game, signature, limit, select);
		if (pointer == 0) {
			return;
		}

		create_hook(name, reinterpret_cast<LPVOID>(pointer), detour, original);
	}

	auto
	null_func() -> void { }

	auto
	return_true() -> bool {
		return true;
	}

	auto
	return_false() -> bool {
		return false;
	}

	auto
	context_log(const char *context, const char *message) -> const char * {
		const auto valid = context != nullptr && context[0] != 0 && context[0] != '?' && message != nullptr && message[0] != 0 && message[0] != '?';
		const auto *result = fwd_context_log(context, message);
		if (valid) {
			const auto current_context = std::string(context);

			if (const auto current_message = std::string(message); current_context != last_context || current_message != last_message) {
				last_context = current_context;
				last_message = current_message;
				g_output << "[ctx] [" << (context == nullptr ? "?" : context) << "] " << (message == nullptr ? "" : message) << "\n";
			}
		}
		return result;
	}

	auto
	log(const char *message, ...) -> void * { // NOLINT(*-dcl50-cpp)
		if (message != nullptr) {
			va_list args; // NOLINT(*-init-variables)
			va_start(args, message);
			const auto buffer_size = vsnprintf(nullptr, 0, message, args) + 1;
			const auto buffer = std::make_unique<char[]>(buffer_size); // NOLINT(*-avoid-c-arrays)
			vsnprintf(buffer.get(), buffer_size, message, args);	   // NOLINT(*-err33-c)
			va_end(args);
			const std::string buffer_str(buffer.get());
			g_output << "[log] " << buffer_str;
			if (buffer_str.back() != '\n') {
				g_output << "\n";
			} else {
			}
		}

		return nullptr;
	}

#pragma clang diagnostic pop

	namespace runtime {
		auto
		init() -> void {
			// this runs on the main thread

			g_output.open("./rivet.log");
			g_output << "[rivet] init\n";
			g_output << "[rivet] version " << RIVET_VERSION << "\n";

			if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, nullptr, &g_game_module)) {
				g_output << "[rivet] unable to get the executable handle.\n";
				return;
			}

			g_settings = Settings::load();
			g_settings.save();

			if (g_settings.suppress_crash_handler) {
				const auto nxe_vtable = load_rel_var(find_address("nxexception", g_game_module, REL_NXEXCEPTION_VTABLE_SIGNATURE), NXEXCEPTION_VTABLE_ADDRESS);
				const auto crash_handler = static_cast<void**>(nxe_vtable)[NXEXCEPTION_VTABLE_INIT];

				create_hook("crash handler", crash_handler, reinterpret_cast<LPVOID>(&null_func), nullptr);
			}

			AssetLoader::init();

			if (g_settings.load_renderdoc) {
				g_output << "[rivet] loading renderdoc\n";
				if (std::filesystem::exists("renderdoc.dll")) {
					g_output << "[rivet] loaded local renderdoc\n";
					g_renderdoc = LoadLibraryA("renderdoc.dll");
				} else {
					if (const auto renderdoc_path = std::filesystem::path(g_settings.renderdoc_path.data()); renderdoc_path.empty()) {
						g_output << "[rivet] renderdoc.dll not found\n";
					} else {
						if (std::filesystem::exists(renderdoc_path)) {
							g_output << "[rivet] loaded " << renderdoc_path << "\n";
							g_renderdoc = LoadLibraryA(g_settings.renderdoc_path.data());
						} else {
							g_output << "[rivet] renderdoc.dll not found\n";
						}
					}
				}
			}

			if (g_settings.dump_ddl) {
				if (g_settings.debug_ddl) {
					std::filesystem::create_directory("./ddl");
				}
				g_output << "[rivet] starting ddl dump thread\n";
				g_ddl_dump_thread = std::thread(ddl::dump_ddl);
			}

			if (g_settings.attach_context_log) {
				create_hook("context log", g_game_module, CONTEXT_LOG_SIGNATURE, reinterpret_cast<LPVOID>(&context_log), reinterpret_cast<LPVOID *>(&fwd_context_log));
			}

			if (g_settings.attach_log) {
				create_hook("log", g_game_module, LOG_SIGNATURE, reinterpret_cast<LPVOID>(&log), nullptr);
			}

			if (g_settings.list_versions) {
				g_output << "[rivet] dumping versions\n";
				ddl::list_versions();
			}

			g_output << "[rivet] init complete\n";
			g_output.flush();
		}

		auto
		fini() -> void {
			g_output << "[rivet] fini\n";

			if (g_renderdoc != nullptr) {
				g_output << "[rivet] unloading renderdoc\n";
				FreeLibrary(g_renderdoc);
			}

			AssetLoader::fini();

			if (g_ddl_dump_thread.joinable()) {
				g_ddl_dump_thread.join();
			}

			g_settings.save();

			g_output << "[rivet] fini complete\n";
			g_output.flush();
			g_output.close();
		}
	} // namespace runtime
} // namespace rivet_hook

#pragma clang diagnostic pop
