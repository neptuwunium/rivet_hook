// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <cstdint>

namespace rivet_hook::game::asset {
	using AssetId = uint64_t;

#pragma pack(push, 1)

	struct Asset {
		void **vtable;
		AssetId assetId;
		const char *name;
		uint16_t nameOffset;
	};

	static_assert(offsetof(Asset, assetId) == 0x8, "Asset assetId offset mismatch");
	static_assert(offsetof(Asset, name) == 0x10, "Asset name offset mismatch");
	static_assert(offsetof(Asset, nameOffset) == 0x18, "Asset nameOffset offset mismatch");

#pragma pack(pop)
} // namespace rivet_hook::game::asset
