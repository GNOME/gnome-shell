shell_toml = configure_file(
  input: 'shell.toml.in',
  output: 'shell.toml',
  configuration: doc_conf
)

custom_target('shell-doc',
  input: [shell_toml, libshell_gir[0]],
  output: 'shell',
  command: [
    gidocgen,
    'generate',
    gi_docgen_common_args,
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../src'),
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../src/st'),
    '--add-include-path=@0@'.format(mutter_dep.get_variable('girdir')),
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../subprojects/gvc'),
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  install: true,
  install_dir: docs_dir,
)
