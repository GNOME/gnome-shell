gnome.compile_resources(
  app_id + '.data',
  app_id + '.data.gresource.xml',
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir
)

desktopconf = configuration_data()
# We substitute in bindir so it works as an autostart
# file when built in a non-system prefix
desktopconf.set('bindir', bindir)
desktopconf.set('app_id', app_id)
desktopconf.set('prgname', prgname)

i18n.merge_file('desktop',
  input: configure_file(
    input: app_id + '.desktop.in.in',
    output: app_id + '.desktop.in',
    configuration: desktopconf
  ),
  output: app_id + '.desktop',
  po_dir: po_dir,
  install: true,
  install_dir: desktopdir,
  type: 'desktop'
)

configure_file(
  input: app_id + '.service.in',
  output: app_id + '.service',
  configuration: desktopconf,
  install_dir: servicedir,
)

subdir('icons')
subdir('metainfo')
