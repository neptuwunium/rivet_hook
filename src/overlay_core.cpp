// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#include <thread>

#include <imgui.h>

#include "game/hero_manager.hpp"
#include "game/scene_manager.hpp"
#include "overlay.hpp"
#include "runtime.hpp"

namespace rivet_hook {
	std::thread g_overlay_init_thread;
	std::thread g_overlay_fini_thread;
	game::HeroSystem *g_HeroManager = nullptr;
	game::SceneManager *g_SceneManager = nullptr;

	auto
	Overlay::init() -> void {
		if (!g_settings.enable_overlay) {
			return;
		}

		g_overlay_init_thread = std::thread(d3d12_init);
	}

	auto
	Overlay::fini() -> void {
		if (!g_settings.enable_overlay) {
			return;
		}

		g_overlay_fini_thread = std::thread(d3d12_fini);
	}

	auto
	Overlay::draw_imgui() -> void {
		ImGui::ShowDemoWindow();
	}
} // namespace rivet_hook
