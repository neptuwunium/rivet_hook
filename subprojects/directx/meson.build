# SPDX-FileCopyrightText: 2025-2026 Neptuwunium
#
# SPDX-License-Identifier: EUPL-1.2

project('directx', 'cpp', meson_version: '>=1.2.1')

compiler = meson.get_compiler('cpp')

dstorage_path = meson.current_source_dir()
dstorage_dep = declare_dependency(
    include_directories: include_directories('include'),
    dependencies: [
        compiler.find_library('dstorage', dirs: dstorage_path + '/lib', required: true)
    ]
)
dxgi_dep = compiler.find_library('dxgi', required: true)
d3d12_dep = compiler.find_library('d3d12', required: true)
