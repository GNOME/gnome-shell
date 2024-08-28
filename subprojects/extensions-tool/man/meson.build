custom_target('gnome-extensions.1',
  input: 'gnome-extensions.rst',
  output: 'gnome-extensions.1',
  command: [rst2man, '--syntax-highlight=none', '@INPUT@'],
  capture: true,
  install_dir: mandir + '/man1',
  install: true
)
