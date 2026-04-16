;;; SPDX-FileCopyrightText: 2025-2026 Neptuwunium
;;;
;;; SPDX-License-Identifier: CC0-1.0

section .data
	extern proc_address

section .text
	global trampoline
	trampoline:
		default rel
		jmp qword [proc_address]
