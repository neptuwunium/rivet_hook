project('directx', 'cpp', meson_version: '>=1.2.1')

compiler = meson.get_compiler('cpp')

dstorage_path = meson.current_source_dir() + '/native'
dstorage_dep = declare_dependency(
    include_directories: include_directories('native/include'),
    dependencies: [
        compiler.find_library('dstorage', dirs: dstorage_path + '/lib', required: true),
        compiler.find_library('dstorage', dirs: dstorage_path + '/bin', required: true),
        compiler.find_library('dstoragecore', dirs: dstorage_path + '/bin', required: true)
    ]
)
