metainfo = app_id + '.metainfo.xml'
i18n.merge_file(metainfo,
  input: metainfo + '.in',
  output: metainfo,
  po_dir: '../../../../po',
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
