// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#include <thread>

#include <imgui.h>
#include <imgui_stdlib.h>

#include "game/hero_manager.hpp"
#include "game/scene_manager.hpp"
#include "overlay.hpp"
#include "runtime.hpp"

#include <mutex>

namespace rivet_hook {
	using namespace game;

	std::thread g_SpawnThread;
	HANDLE g_SpawnSignal;

	HeroSystem *g_HeroManager = nullptr;
	SceneManager *g_SceneManager = nullptr;
	constexpr intptr_t ACTOR_ASSET_MANAGER = 0x1452e0b80;

	using SpawnBot_t = void (*)(intptr_t self, Asset *actorAsset);
	using LoadActorAsset_t = Asset * (*)(intptr_t self, const char* name, Asset* loadedFrom, char const* loadInfo);

	const auto game_SpawnBot = reinterpret_cast<SpawnBot_t>(0x1403b3450);
	const auto game_LoadActorAsset = reinterpret_cast<LoadActorAsset_t>(0x140f17f00);

	char debugSpawnActorPath[0x200];
	Asset *debugSpawnActor = nullptr;
	bool isSpawningDebugActor = false;

	auto
	SpawnDebugActor() -> void {
		while (true) {
			if (FAILED(WaitForSingleObject(g_SpawnSignal, INFINITE))) {
				break;
			}

			{
				if (debugSpawnActor == nullptr || debugSpawnActor->status != AssetStatus::Loaded || isSpawningDebugActor) {
					continue;
				}

				isSpawningDebugActor = true;
				game_SpawnBot(0, debugSpawnActor);
				isSpawningDebugActor = false;
			}
		}
	}

	auto
	Overlay::HandleKeyPress(const int vk) -> void {
		if (vk == g_settings.overlay.spawn_debug_actor_key) {
			SetEvent(g_SpawnSignal);
		}
	}

	auto
	DrawDebugSpawn() -> void {
		const auto isDebugActorLoading = debugSpawnActor != nullptr && debugSpawnActor->status < AssetStatus::Loaded;
		const auto isDebugActorInvalid = debugSpawnActor == nullptr || debugSpawnActor->status != AssetStatus::Loaded || isSpawningDebugActor;
		const auto shouldHideInput = isDebugActorLoading || isSpawningDebugActor;

		ImGui::LabelText("Actor Load Status", "%d", static_cast<int32_t>(debugSpawnActor ? debugSpawnActor->status : AssetStatus::Invalid));
		ImGui::InputText("Actor Path", debugSpawnActorPath, sizeof(debugSpawnActorPath), shouldHideInput ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);

		ImGui::BeginDisabled(isDebugActorLoading);
		if (ImGui::Button("Load")) {
			debugSpawnActor = game_LoadActorAsset(ACTOR_ASSET_MANAGER, debugSpawnActorPath, debugSpawnActor, nullptr);
		}
		ImGui::EndDisabled();

		ImGui::BeginDisabled(isDebugActorInvalid);
		if (ImGui::Button("Spawn")) {
			SetEvent(g_SpawnSignal);
		}
		ImGui::EndDisabled();
	}

	auto
	Overlay::DrawImGUI() -> void {
		ImGui::Begin("Rivet");

		DrawDebugSpawn();

		ImGui::End();
	}

	auto
	Overlay::Init() -> void {
		if (!g_settings.overlay.enabled) {
			return;
		}

		memset(debugSpawnActorPath, 0, sizeof(debugSpawnActorPath));
		g_SpawnSignal = CreateEvent(nullptr, false, false, "Rivet Debug Spawn Signal");
		g_SpawnThread = std::thread(SpawnDebugActor);

		std::thread(D3D12Init).detach();
	}

	auto
	Overlay::Fini() -> void {
		if (!g_settings.overlay.enabled) {
			return;
		}

		CloseHandle(g_SpawnSignal);
		D3D12Fini();
	}
} // namespace rivet_hook
