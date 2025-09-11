subdir('icons')
subdir('metainfo')

gnome.compile_resources(
  app_id + '.data',
  configure_file(
    input: base_id + '.data.gresource.xml.in',
    output: app_id + '.data.gresource.xml',
    configuration: {
      'app_id': app_id,
      'profile': '/'.join(profile.split('.')),
    },
  ),
  dependencies: [metainfo],
  source_dir: ['.', meson.current_build_dir()],
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir
)

desktop_id = app_id + '.desktop'
desktopconf = configuration_data()
# We substitute in bindir so it works as an autostart
# file when built in a non-system prefix
desktopconf.set('bindir', bindir)
desktopconf.set('app_id', app_id)
desktopconf.set('prgname', prgname)

desktop_file = i18n.merge_file(
  input: configure_file(
    input: base_id + '.desktop.in.in',
    output: desktop_id + '.in',
    configuration: desktopconf
  ),
  output: desktop_id,
  po_dir: po_dir,
  install: true,
  install_dir: desktopdir,
  type: 'desktop'
)

if (desktop_file_validate.found())
  test('Validating ' + desktop_id,
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

install_data(base_id + '.gschema.xml', install_dir: schemadir)
