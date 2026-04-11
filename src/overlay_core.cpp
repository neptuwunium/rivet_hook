// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#include "imgui.h"
#include "overlay.hpp"

namespace rivet_hook {
	auto
	Overlay::draw_imgui() -> void {
		ImGui::ShowDemoWindow();
	}
}