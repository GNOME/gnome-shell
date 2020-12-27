custom_target('man page',
  input: ['gnome-shell.txt', 'stylesheet.xsl'],
  output: 'gnome-shell.1',
  command: [a2x, '-D', '@OUTDIR@', '--xsl-file', '@INPUT1@', '-f' ,'manpage', '@INPUT0@'],
  install_dir: mandir + '/man1',
  install: true
)
