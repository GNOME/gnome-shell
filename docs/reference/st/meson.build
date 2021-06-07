st_toml = configure_file(
  input: 'st.toml.in',
  output: 'st.toml',
  configuration: doc_conf
)

custom_target('st-doc',
  input: [st_toml, libst_gir[0]],
  output: 'st',
  command: [
    gidocgen,
    'generate',
    gi_docgen_common_args,
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../src'),
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../src/st'),
    '--add-include-path=@0@'.format(mutter_dep.get_variable('girdir')),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  install: true,
  install_dir: docs_dir,
)
