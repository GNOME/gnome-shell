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


dbus_interfaces = [
  'org.gnome.Shell.PadOsd.xml',
  'org.gnome.Shell.Screencast.xml',
  'org.gnome.Shell.Screenshot.xml',
  'org.gnome.ShellSearchProvider.xml',
  'org.gnome.ShellSearchProvider2.xml'
]
install_data(dbus_interfaces, install_dir: ifacedir)


subdir('theme')

theme_resources = gnome.compile_resources(
  'gnome-shell-theme', 'gnome-shell-theme.gresource.xml',
  source_dir: 'theme',
  dependencies: theme_deps,
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


install_data('50-gnome-shell-system.xml', install_dir: keysdir)


schemaconf = configuration_data()
schemaconf.set('GETTEXT_PACKAGE', meson.project_name())
configure_file(
  input: 'org.gnome.shell.gschema.xml.in',
  output: 'org.gnome.shell.gschema.xml',
  configuration: schemaconf,
  install_dir: schemadir
)

install_data('gnome-shell-overrides.convert', install_dir: convertdir)
