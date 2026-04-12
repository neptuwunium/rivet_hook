// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <cstdint>

#include "actor.hpp"

namespace rivet_hook::game {
#pragma pack(push, 1)

	struct SceneManager { };

	extern SceneManager *g_SceneManager;

#pragma pack(pop)
} // namespace rivet_hook::game
