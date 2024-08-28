custom_target('man page',
  input: 'gnome-shell.rst',
  output: 'gnome-shell.1',
  command: [rst2man, '--syntax-highlight=none', '@INPUT@'],
  capture: true,
  install_dir: mandir + '/man1',
  install: true
)
