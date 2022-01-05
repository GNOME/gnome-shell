gnome.compile_resources(
  app_id + '.data',
  configure_file(
    input: base_id + '.data.gresource.xml.in',
    output: app_id + '.data.gresource.xml',
    configuration: {'profile': '/'.join(profile.split('.')) },
  ),
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir
)

desktop_file = app_id + '.desktop'
desktopconf = configuration_data()
# We substitute in bindir so it works as an autostart
# file when built in a non-system prefix
desktopconf.set('bindir', bindir)
desktopconf.set('app_id', app_id)
desktopconf.set('prgname', prgname)

i18n.merge_file(
  input: configure_file(
    input: base_id + '.desktop.in.in',
    output: desktop_file + '.in',
    configuration: desktopconf
  ),
  output: desktop_file,
  po_dir: po_dir,
  install: true,
  install_dir: desktopdir,
  type: 'desktop'
)

if (desktop_file_validate.found())
  test('Validating ' + desktop_file,
    desktop_file_validate,
    args: [desktop_file],
    workdir: meson.current_build_dir()
  )
endif

configure_file(
  input: base_id + '.service.in',
  output: app_id + '.service',
  configuration: desktopconf,
  install_dir: servicedir,
)

subdir('icons')
subdir('metainfo')
