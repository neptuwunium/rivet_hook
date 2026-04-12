// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <cstdint>

#include "actor.hpp"

namespace rivet_hook::game {
#pragma pack(push, 1)

	struct Hero {
		EngineHandle componentHandle;
		EngineHandle actorHandle;
		uint32_t characterId;
		uint32_t unknown1;
		uint32_t unknown2;
	};

	static_assert(sizeof(Hero) == 0x14, "Hero size is not 0x1c");

	struct HeroSystem {
		intptr_t vtable;
		EngineHandle actorHandle;
		EngineHandle unknownHandle;
		uint64_t unknown1;
		Hero heroes[0x10];
		Hero *slots[2];
		EngineHandle currentActorHandle;
		uint32_t unknown2;
		EngineHandle gadgetHandles[0x10];
		uint32_t unknown3;
	};

	static_assert(sizeof(HeroSystem) == 0x1b4, "HeroSystem size is not 0x1b4");

#pragma pack(pop)

	extern HeroSystem *g_HeroSystem;
} // namespace rivet_hook::game
