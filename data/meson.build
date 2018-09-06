desktop_files = [
  'org.gnome.Shell.desktop',
  'gnome-shell-extension-prefs.desktop'
]
service_files = []

if have_networkmanager
  desktop_files += 'org.gnome.Shell.PortalHelper.desktop'
  service_files += 'org.gnome.Shell.PortalHelper.service'
endif

desktopconf = configuration_data()
# We substitute in bindir so it works as an autostart
# file when built in a non-system prefix
desktopconf.set('bindir', bindir)
desktopconf.set('VERSION', meson.project_version())
foreach desktop_file : desktop_files
  i18n.merge_file('desktop',
    input: configure_file(
      input: desktop_file + '.in.in',
      output: desktop_file + '.in',
      configuration: desktopconf
    ),
    output: desktop_file,
    po_dir: '../po',
    install: true,
    install_dir: desktopdir,
    type: 'desktop'
  )
endforeach

serviceconf = configuration_data()
serviceconf.set('libexecdir', libexecdir)
foreach service_file : service_files
  configure_file(
    input: service_file + '.in',
    output: service_file,
    configuration: serviceconf,
    install_dir: servicedir
  )
endforeach


subdir('dbus-interfaces')
subdir('theme')

theme_resources = gnome.compile_resources(
  'gnome-shell-theme', 'gnome-shell-theme.gresource.xml',
  source_dir: 'theme',
  dependencies: theme_deps,
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir
)

osk_layout_resources = gnome.compile_resources(
  'gnome-shell-osk-layouts', 'gnome-shell-osk-layouts.gresource.xml',
  source_dir: 'osk-layouts',
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir
)

perfconf = configuration_data()
perfconf.set('datadir', datadir)
configure_file(
  input: 'perf-background.xml.in',
  output: 'perf-background.xml',
  configuration: perfconf,
  install_dir: pkgdatadir
)


install_data('gnome-shell.portal', install_dir: portaldir)
install_data('50-gnome-shell-system.xml', install_dir: keysdir)


schemaconf = configuration_data()
schemaconf.set('GETTEXT_PACKAGE', meson.project_name())
schema = configure_file(
  input: 'org.gnome.shell.gschema.xml.in',
  output: 'org.gnome.shell.gschema.xml',
  configuration: schemaconf,
  install_dir: schemadir
)
install_data('00_org.gnome.shell.gschema.override', install_dir: schemadir)

overrides_migration_conf = configuration_data()
overrides_migration_conf.set('libexecdir', libexecdir)
overrides_migration = configure_file(
  input: 'gnome-shell-overrides-migration.desktop.in',
  output: 'gnome-shell-overrides-migration.desktop',
  configuration: overrides_migration_conf,
  install_dir: autostartdir
)

if have_systemd
  unitconf = configuration_data()
  unitconf.set('bindir', bindir)

  unit = configure_file(
    input: 'gnome-shell.service.in',
    output: 'gnome-shell.service',
    configuration: unitconf,
    install_dir: systemduserunitdir
  )

  units = files('gnome-shell-wayland.target',
                'gnome-shell-x11.target')

  install_data(units, install_dir: systemduserunitdir)
endif

# for unit tests - gnome.compile_schemas() only looks in srcdir
custom_target('compile-schemas',
  input: schema,
  output: 'gschemas.compiled',
  command: [find_program('glib-compile-schemas'), meson.current_build_dir()],
  build_by_default: true)

install_data('gnome-shell-overrides.convert', install_dir: convertdir)
