# SPDX-FileCopyrightText: 2025-2026 Neptuwunium
#
# SPDX-License-Identifier: EUPL-1.2

project('rivet_hid', 'cpp',
	license: 'EUPL-1.2',
	version: '1.0.0',
	default_options: [
		'prefix=/usr',
		'cpp_std=c++20'
	],
    meson_version: '>=1.2.1')

ext = 'asm'

if not add_languages('nasm', native: false, required: true)
    error('nasm not available')
endif

args = meson.get_compiler('cpp').get_supported_arguments('-static-libgcc', '-static-libstdc++')

shared_library('hid', [
        'src/main.cpp',
        'src/trampoline.' + ext
    ],
    name_prefix: '',
    link_args: args,
    vs_module_defs: 'src/hid11.def',
)

shared_library('hid_win10', [
        'src/main.cpp',
        'src/trampoline.' + ext
    ],
    name_prefix: '',
    link_args: args,
    vs_module_defs: 'src/hid10.def'
)
