metainfo = app_id + '.metainfo.xml'
i18n.merge_file(metainfo,
  input: metainfo + '.in',
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

  r = run_command(appstream_util, '--help')
  if (r.stdout().contains('validate-version'))
    test('Checking release entry for @0@'.format(meson.project_version()),
      appstream_util,
      args: ['validate-version', metainfo, meson.project_version()],
      workdir: meson.current_build_dir()
    )
  endif
endif
