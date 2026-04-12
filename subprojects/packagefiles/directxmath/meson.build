project(
  'directxmath',
  'cpp',
  version: '2024.10.15.1',
)

incdirs = include_directories('include')

directxmath_headers = files(
  'include/DirectXCollision.h',
  'include/DirectXCollision.inl',
  'include/DirectXColors.h',
  'include/DirectXMath.h',
  'include/DirectXMathConvert.inl',
  'include/DirectXMathMatrix.inl',
  'include/DirectXMathMisc.inl',
  'include/DirectXMathVector.inl',
  'include/DirectXPackedVector.h',
  'include/DirectXPackedVector.inl',
)

directxmath_dep = declare_dependency(
  include_directories: incdirs,
)

install_headers(
  directxmath_headers,
  subdir: 'directxmath',
)

pkgconfig = import('pkgconfig')
pkgconfig.generate(
  name: 'directxmath',
  version: meson.project_version(),
  subdirs: 'directxmath',
  description: 'DirectXMath SIMD C++ math library',
)

