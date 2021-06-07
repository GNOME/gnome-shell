gidocgen = find_program('gi-docgen', required: get_option('gtk_doc'))

doc_conf = configuration_data()
doc_conf.set('SHELL_VERSION', meson.project_version())

shell_toml = configure_file(input: 'shell.toml.in',
                            output: 'shell.toml',
                            configuration: doc_conf)

custom_target('shell-doc',
  input: libshell_gir[0],
  output: 'Shell',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../src'),
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../src/st'),
    '--add-include-path=@0@'.format(mutter_dep.get_pkgconfig_variable('girdir')),
    '--add-include-path=@0@'.format(meson.build_root() / 'subprojects/gvc'),
    '--config', shell_toml,
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT@',
  ],
  depend_files: [ shell_toml ],
  build_by_default: true,
  install: true,
  install_dir: datadir / 'doc/shell/reference',
)

st_toml = configure_file(input: 'st.toml.in',
                         output: 'st.toml',
                         configuration: doc_conf)

custom_target('st-doc',
  input: libst_gir[0],
  output: 'St',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../src'),
    '--add-include-path=@0@'.format(meson.current_build_dir() / '../src/st'),
    '--add-include-path=@0@'.format(mutter_dep.get_pkgconfig_variable('girdir')),
    '--config', st_toml,
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--fatal-warnings',
    '@INPUT@',
  ],
  depend_files: [ st_toml ],
  build_by_default: true,
  install: true,
  install_dir: datadir / 'doc/st/reference',
)
