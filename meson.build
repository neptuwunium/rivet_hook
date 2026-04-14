# SPDX-FileCopyrightText: 2025-2026 Neptuwunium
#
# SPDX-License-Identifier: EUPL-1.2

project('rivet', 'cpp',
	license: 'EUPL-1.2',
	version: '1.1.0',
	default_options: [
		'prefix=/usr',
		'cpp_std=c++20'
	],
    meson_version: '>=1.2.1')

compiler = meson.get_compiler('cpp')

cpp_args = []

cpp_args = compiler.get_supported_arguments(
    '/wd4061',
    '/wd4068',
    '/wd4070',
    '/wd4201',
    '/wd4365',
    '/wd4464',
    '/wd4582',
    '/wd4583',
    '/wd4623',
    '/wd4625',
    '/wd4626',
    '/wd4668',
    '/wd4710',
    '/wd4711',
    '/wd4820',
    '/wd4866',
    '/wd5027',
    '/wd5039',
    '/wd5045',
    '/wd5267',
    '/utf-8',
    '/EHsc',
    '-fPIC',
    '-finput-charset=utf-8',
    '-fexec-charset=utf-8',
    '-faddrsig',
    '-faligned-new',
    '-Wno-unknown-pragmas',
    '-Wall',
    '-Wextra',
    '-Wno-pedantic',
    '-Wno-unused-but-set-variable',
    '-Wno-unused-variable',
    '-Wno-unused-parameter',
    '-Wno-deprecated-literal-operator',
)

minhook_dep = dependency('minhook', version: '>= 1.3.3')

nlohmann_json_dep = dependency('', required: false)
if not compiler.check_header('nlohmann/json.hpp')
    nlohmann_json_dep = dependency('nlohmann_json', version: '>= 3.11.2')
endif

toml11_dep = dependency('', required: false)
if not compiler.check_header('toml.hpp')
    toml11_dep = dependency('toml11', version: '>= 4.4.0')
endif

# imgui has no stable api, can't rely on system headers, we also are the only one disabling obsolete functions
imgui_dep = subproject('imgui-docking',
   default_options: {
       'dx12': 'enabled',
       'win': 'enabled',
       'disable_obsolete_functions': true,
   },
   required: true).get_variable('imgui_dep')

dstorage_dep = dependency('directstorage', version: '= 1.2.2')
dxgi_dep = compiler.find_library('dxgi', required: true)
d3d12_dep = compiler.find_library('d3d12', required: true)
directxmath_dep = dependency('directxmath', version: '>= 2024.10.15.1')

subproject('rivet_hid')

rivet_hook = shared_library('rivet_hook', [
        'src/ddl.cpp',
        'src/main.cpp',
        'src/overlay.cpp',
        'src/overlay_core.cpp',
        'src/runtime.cpp',
        'src/runtime_loader.cpp',
        'src/settings.cpp',
        'src/signature_engine.cpp',
        'src/vk_enum.cpp',
    ],
    link_args: compiler.get_supported_arguments('-static-libgcc', '-static-libstdc++'),
    dependencies: [
        minhook_dep,
        nlohmann_json_dep,
        toml11_dep,
        dstorage_dep,
        imgui_dep,
        dxgi_dep,
        d3d12_dep,
        directxmath_dep
    ],
    name_prefix: '',
    cpp_args: cpp_args
)
