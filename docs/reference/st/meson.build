private_headers = [
  'st-private.h',
  'st-theme-node-private.h'
]

configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: version_conf
)

gnome.gtkdoc('st',
  main_sgml: 'st-docs.sgml',
  src_dir: [
    join_paths(meson.project_source_root(), 'src', 'st'),
    join_paths(meson.project_build_root(), 'src', 'st')
  ],
  scan_args: [
    '--ignore-headers=' + ' '.join(private_headers),
    '--rebuild-sections'
  ],
  install: true
)
