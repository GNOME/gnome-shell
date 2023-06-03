project('gnome-extensions-tool', 'c',
  version: '44.2',
  meson_version: '>= 0.58.0',
  license: 'GPLv2+'
)

gio_req = '>= 2.56.0'

fs = import('fs')
gnome = import('gnome')
i18n  = import('i18n')

if meson.is_subproject()
  package_name = get_option('package_name')
  assert(package_name != '',
    'package_name must be specified for subproject builds')
else
  package_name = meson.project_name()
endif

package_version = meson.project_version()
prefix = get_option('prefix')

bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
mandir = join_paths(prefix, get_option('mandir'))

localedir = join_paths(datadir, 'locale')

gio_dep = dependency('gio-2.0', version: gio_req)
gio_unix_dep = dependency('gio-unix-2.0', version: gio_req)
autoar_dep = dependency('gnome-autoar-0')
json_dep = dependency('json-glib-1.0')

cc = meson.get_compiler('c')

bash_completion = dependency('bash-completion', required: get_option('bash_completion'))

po_dir = meson.global_source_root() + '/po'

subdir('src')

if bash_completion.found()
  install_data('completion/bash/gnome-extensions',
    install_dir: bash_completion.get_variable('completionsdir', pkgconfig_define: ['datadir', datadir])
  )
endif

if get_option('man')
  if fs.exists('man/gnome-extensions.1')
    install_man('man/gnome-extensions.1')
  else
    a2x = find_program('a2x')
    subdir('man')
  endif
endif

if not meson.is_subproject()
  subdir('po')

  summary_dirs = {
    'prefix': get_option('prefix'),
    'bindir': get_option('bindir'),
    'datadir': get_option('datadir'),
  }

  if get_option('man')
    summary_dirs += { 'mandir': get_option('mandir') }
  endif

  summary_build = {
    'buildtype': get_option('buildtype'),
    'debug': get_option('debug'),
  }

  summary_options = {
    'man': get_option('man'),
    'bash_completion': bash_completion.found(),
  }

  summary(summary_dirs, section: 'Directories')
  summary(summary_build, section: 'Build Configuration')
  summary(summary_options, section: 'Build Options')
endif
