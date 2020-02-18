custom_target('gnome-extensions.1',
  input: 'gnome-extensions.adoc',
  output: 'gnome-extensions.1',
  command: [asciidoctor, '-D', '@OUTDIR@', '-b', 'manpage', '@INPUT@'],
  install_dir: mandir + '/man1',
  install: true
)
