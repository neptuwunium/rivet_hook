// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <array>
#include <cstdint>

#include <nlohmann/json.hpp>

#include "game/ddl.hpp"

namespace rivet_hook::ddl {
	auto
	get_ddl_field(nlohmann::json &field, const uint8_t *object, uint32_t offset, uint8_t array_type, uint8_t field_type, int32_t index, const game::ddl::ddl_type_info *type_ptr, int32_t type_index) -> void;
	auto
	dump_ddl() -> void;
	auto
	list_versions() -> void;
} // namespace rivet_hook::ddl
