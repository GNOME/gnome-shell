config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name())
config_h.set_quoted('VERSION', meson.project_version())
config_h.set_quoted('LOCALEDIR', localedir)
config_h.set('HAVE_BIND_TEXTDOMAIN_CODESET', cc.has_function('bind_textdomain_codeset'))
configure_file(
  output: 'config.h',
  configuration: config_h,
)

sources = [
  'command-disable.c',
  'command-enable.c',
  'main.c'
]

executable('gnome-extensions',
  sources,
  dependencies: gio_dep,
  install: true
)
