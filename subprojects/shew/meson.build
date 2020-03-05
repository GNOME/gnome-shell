project('shew', 'c',
  version: '3.36.0',
  meson_version: '>= 0.47.0',
  license: 'LGPLv2+',
)

gnome = import('gnome')
pkg = import('pkgconfig')

api_version = '0'
full_name = '@0@-@1@'.format(meson.project_name(), api_version)

if meson.is_subproject()
  package_version = meson.project_version()
  package_name = get_option('package_name')
  assert(package_name != '',
    'package_name must be specified for subproject builds')

  pkgdatadir = join_paths(get_option('datadir'), package_name)
  pkglibdir = join_paths(get_option('libdir'), package_name)

  libdir = pkglibdir
  girbase = pkgdatadir
  typelibbase = pkglibdir
else
  libdir = get_option('libdir')
  datadir = get_option('datadir')

  girbase = datadir
  typelibbase = libdir
endif

girdir = join_paths(girbase, 'gir-1.0')
typelibdir = join_paths(typelibbase, 'girepository-1.0')

gtk_dep = dependency('gtk+-3.0')

subdir('src')

if not meson.is_subproject()
  install_headers(shew_public_headers, subdir: full_name)

  pkg.generate(
    filebase: full_name,
    name: 'SHEW',
    description: 'GNOME Shell External Windows',
    libraries: libshew,
    requires: gtk_dep,
    subdirs: full_name,
  )

  summary_dirs = {
    'prefix': get_option('prefix'),
    'libdir': get_option('libdir'),
    'datadir': get_option('datadir'),
  }

  summary_build = {
    'buildtype': get_option('buildtype'),
    'debug': get_option('debug'),
  }

  if meson.version().version_compare('>= 0.53.0')
    summary(summary_dirs, section: 'Directories')
    summary(summary_build, section: 'Build Configuration')
  endif
endif
