data_builddir = meson.current_build_dir()

desktop_files = [
  'org.gnome.Shell.desktop',
  'org.gnome.Shell.Extensions.desktop',
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
desktopconf.set('systemd_hidden', have_systemd ? 'true' : 'false')

foreach desktop_file : desktop_files
  i18n.merge_file(
    input: configure_file(
      input: desktop_file + '.in.in',
      output: desktop_file + '.in',
      configuration: desktopconf
    ),
    output: desktop_file,
    po_dir: po_dir,
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

theme_deps = []

subdir('dbus-interfaces')
subdir('icons')
subdir('theme')

data_resources = [
  {'name': 'dbus-interfaces'},
  {'name': 'icons'},
  {'name': 'osk-layouts'},
  {'name': 'theme', 'deps': theme_deps}
]
foreach resource : data_resources
  name = resource.get('name')
  deps = resource.get('deps', [])

  gnome.compile_resources(
    'gnome-shell-' + name,
    'gnome-shell-@0@.gresource.xml'.format(name),
    source_dir: name,
    dependencies: deps,
    gresource_bundle: true,
    install: true,
    install_dir: pkgdatadir
  )
endforeach

perfconf = configuration_data()
perfconf.set('datadir', datadir)
configure_file(
  input: 'perf-background.xml.in',
  output: 'perf-background.xml',
  configuration: perfconf,
  install_dir: pkgdatadir
)

keybinding_files = [
  '50-gnome-shell-launchers.xml',
  '50-gnome-shell-screenshots.xml',
  '50-gnome-shell-system.xml',
]

install_data('gnome-shell.portal', install_dir: portaldir)
install_data(keybinding_files, install_dir: keysdir)


schemaconf = configuration_data()
schemaconf.set('GETTEXT_PACKAGE', meson.project_name())
schema = configure_file(
  input: 'org.gnome.shell.gschema.xml.in',
  output: 'org.gnome.shell.gschema.xml',
  configuration: schemaconf,
  install_dir: schemadir
)
install_data('00_org.gnome.shell.gschema.override', install_dir: schemadir)

if have_systemd
  unitconf = configuration_data()
  unitconf.set('bindir', bindir)

  configure_file(
    input: 'org.gnome.Shell@x11.service.in',
    output: 'org.gnome.Shell@x11.service',
    configuration: unitconf,
    install_dir: systemduserunitdir
  )

  configure_file(
    input: 'org.gnome.Shell@wayland.service.in',
    output: 'org.gnome.Shell@wayland.service',
    configuration: unitconf,
    install_dir: systemduserunitdir
  )

  units = files('org.gnome.Shell.target',
                'org.gnome.Shell-disable-extensions.service')

  install_data(units, install_dir: systemduserunitdir)
endif

# for unit tests - gnome.compile_schemas() only looks in srcdir
custom_target('compile-schemas',
  input: schema,
  output: 'gschemas.compiled',
  command: [find_program('glib-compile-schemas'), data_builddir],
  build_by_default: true)
