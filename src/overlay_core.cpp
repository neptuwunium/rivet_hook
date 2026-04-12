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
	using namespace game;
	std::thread g_overlay_init_thread;
	std::thread g_overlay_fini_thread;
	HeroSystem *g_HeroManager = nullptr;
	SceneManager *g_SceneManager = nullptr;

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

	Asset *testAsset = nullptr;

	using spawn_bot_t = void (*)(intptr_t self, Asset *actorAsset);
	using load_actor_asset_t = Asset * (*)(intptr_t self, const char* name, Asset* loadedFrom, char const* loadInfo);

	const auto spawn_bot = reinterpret_cast<spawn_bot_t>(0x1403b3450);
	const auto load_actor_asset = reinterpret_cast<load_actor_asset_t>(0x140f17f00);
	constexpr intptr_t ACTOR_ASSET_MANAGER = 0x1452e0b80;
	bool loading = false;

	auto
	Overlay::draw_imgui() -> void {
		ImGui::Begin("Rivet");

		ImGui::LabelText("load status", "%d", static_cast<int32_t>(testAsset ? testAsset->status : AssetStatus::Invalid));
		if (!testAsset) {
			if (ImGui::Button("Scary Button")) {
				testAsset = load_actor_asset(ACTOR_ASSET_MANAGER, "characters/hero/hero_spidertank_micro/hero_glitch_gallery.actor", testAsset, nullptr);
			}
		} else {
			ImGui::BeginDisabled(testAsset->status != AssetStatus::Loaded || loading);
			if (ImGui::Button("Scarier Button")) {
				loading = true;
				std::thread([]{
					if (testAsset) {
						spawn_bot(0, testAsset);
					}
					loading = false;
				}).detach();
			}
			ImGui::EndDisabled();
		}

		ImGui::End();
	}
} // namespace rivet_hook
