custom_target('man page',
  input: 'gnome-shell.xml',
  output: 'gnome-shell.1',
  command: [
    xsltproc,
    '-o', '@OUTPUT@',
    '--nonet',
    '--stringparam', 'man.output.quietly', '1',
    '--stringparam', 'funcsynopsis.style', 'ansi',
    '--stringparam', 'man.th.extra1.suppress', '1',
    '--stringparam', 'man.authors.section.enabled', '0',
    '--stringparam', 'man.copyright.section.enabled', '0',
    'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
    '@INPUT@'
  ],
  build_by_default: true,
  install_dir: mandir + '/man1',
  install: true
)
