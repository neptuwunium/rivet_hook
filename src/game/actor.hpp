// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <cstdint>

namespace rivet_hook::game::actor {
#pragma pack(push, 1)

	struct ActorHandleMetadata {
		uint32_t id : 20;
		uint32_t type : 12;
	};
	static_assert(sizeof(ActorHandleMetadata) == 4, "ActorHandleMetadata size is not 4");

	struct ActorHandle {
		union {
			ActorHandleMetadata metadata;
			uint32_t value;
		};
	};
	static_assert(sizeof(ActorHandle) == 4, "ActorHandle size is not 4");

#pragma pack(pop)
}