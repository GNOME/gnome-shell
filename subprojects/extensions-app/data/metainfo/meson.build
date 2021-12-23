metainfo = app_id + '.metainfo.xml'
i18n.merge_file(
  input: base_id + '.metainfo.xml.in',
  output: metainfo,
  po_dir: po_dir,
  install: true,
  install_dir: metainfodir
)

if (appstream_util.found())
  test('Validating ' + metainfo,
    appstream_util,
    args: ['validate', '--nonet', metainfo],
    workdir: meson.current_build_dir()
  )
endif
