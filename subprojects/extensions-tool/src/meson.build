config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', package_name)
config_h.set_quoted('VERSION', meson.project_version())
config_h.set_quoted('LOCALEDIR', localedir)
config_h.set('HAVE_BIND_TEXTDOMAIN_CODESET', cc.has_function('bind_textdomain_codeset'))
configure_file(
  output: 'config.h',
  configuration: config_h,
)

sources = [
  'command-create.c',
  'command-disable.c',
  'command-enable.c',
  'command-info.c',
  'command-install.c',
  'command-list.c',
  'command-pack.c',
  'command-prefs.c',
  'command-reset.c',
  'command-uninstall.c',
  'command-upload.c',
  'main.c'
]

subdir('templates')

resources = gnome.compile_resources('resources',
  'gnome-extensions-tool.gresource.xml',
  source_dir: ['.', meson.current_build_dir()],
  dependencies: template_deps,
)

executable('gnome-extensions',
  sources, resources,
  dependencies: [gio_dep, gio_unix_dep, autoar_dep, json_dep, soup_dep],
  install: true
)
