// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

namespace rivet_hook {
	struct Overlay {
		static auto
		Init() -> void;

		static auto
		D3D12Init() -> void;

		static auto
		Fini() -> void;

		static auto
		D3D12Fini() -> void;

		static auto
		HandleKeyPress(int vk) -> void;

		static auto
		DrawImGUI() -> void;
	};
}
