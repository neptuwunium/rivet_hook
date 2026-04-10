// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

namespace rivet_hook {
	struct AssetLoader {
		static auto
		init() -> void;
		static auto
		fini() -> void;
	};
} // namespace rivet_hook
