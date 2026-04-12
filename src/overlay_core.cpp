// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#include <thread>

#include <imgui.h>

#include "overlay.hpp"
#include "game/hero_manager.hpp"
#include "game/scene_manager.hpp"

namespace rivet_hook {
	std::thread g_overlay_init_thread;
	std::thread g_overlay_fini_thread;

	auto
	Overlay::init() -> void {
		g_overlay_init_thread = std::thread(d3d12_init);
	}

	auto
	Overlay::fini() -> void {
		g_overlay_fini_thread = std::thread(d3d12_fini);
	}

	auto
	Overlay::draw_imgui() -> void {
		ImGui::ShowDemoWindow();
	}
}