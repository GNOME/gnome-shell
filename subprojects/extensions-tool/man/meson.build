custom_target('gnome-extensions.1',
  input: ['gnome-extensions.txt', 'stylesheet.xsl'],
  output: 'gnome-extensions.1',
  command: [a2x, '-D', '@OUTDIR@', '--xsl-file', '@INPUT1@', '-f', 'manpage', '@INPUT0@'],
  install_dir: mandir + '/man1',
  install: true
)
