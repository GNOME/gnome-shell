metainfoconf = configuration_data()
metainfoconf.set('app_id', app_id)
metainfoconf.set('package_name', package_name)

metainfo = i18n.merge_file(
  input: configure_file(
    input: base_id + '.metainfo.xml.in',
    output: app_id + '.metainfo.xml.in',
    configuration: metainfoconf,
  ),
  output: '@BASENAME@',
  po_dir: po_dir,
  install: true,
  install_dir: metainfodir
)

if (appstream_util.found())
  test('Validating metainfo via appstream_util',
    appstream_util,
    args: ['validate', '--nonet', metainfo],
    workdir: meson.current_build_dir()
  )
endif

# Validate Appdata
if (appstreamcli.found())
  test('Validating metainfo',
    appstreamcli,
    args: ['validate', '--no-net', metainfo],
    workdir: meson.current_build_dir()
  )
endif
