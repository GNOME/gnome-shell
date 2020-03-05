project('gnome-extensions-tool', 'c',
  version: '3.36.0',
  meson_version: '>= 0.47.0',
  license: 'GPLv2+'
)

gio_req = '>= 2.56.0'

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

subdir('src')

if bash_completion.found()
  install_data('completion/bash/gnome-extensions',
    install_dir: bash_completion.get_pkgconfig_variable('completionsdir')
  )
endif

if get_option('man')
  a2x = find_program('a2x')

  subdir('man')
endif

if not meson.is_subproject()
  subdir('po')
endif
