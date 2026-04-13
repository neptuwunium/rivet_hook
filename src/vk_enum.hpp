// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <string>
#include <unordered_map>

extern std::unordered_map<std::string, int> StringToVKeyLookup;
extern std::unordered_map<int, std::string> VKeyToStringLookup;

auto
StringToVKey(const std::string &str) -> int;

auto
VKeyToString(int value) -> std::string;