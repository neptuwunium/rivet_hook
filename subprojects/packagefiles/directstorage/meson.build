project('directx', 'cpp', version: '1.2.2', meson_version: '>=1.2.1')

compiler = meson.get_compiler('cpp')

directstorage_path = meson.current_source_dir() + '/native'
directstorage_dep = declare_dependency(
    include_directories: include_directories('native/include'),
    dependencies: [
        compiler.find_library('dstorage', dirs: directstorage_path + '/lib/x64', required: true),
    ]
)
