// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#ifdef IM_RELEASE
#define IM_ASSERT(_EXPR) ((void)(_EXPR))
#else
#include <windows.h>
#define IM_ASSERT(_EXPR) (!(_EXPR) && MessageBoxA(NULL, #_EXPR, "Assertion Failed", MB_OK | MB_ICONERROR))
#endif
