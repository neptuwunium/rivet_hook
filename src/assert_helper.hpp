// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#define size_assert(struc, size) static_assert(sizeof(struc) == size, #struc " is not " #size " bytes")
#define offset_assert(struc, field, offset) static_assert(offsetof(struc, field) == offset, #struc "." #field " is not at offset " #offset)