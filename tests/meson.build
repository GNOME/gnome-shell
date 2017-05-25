testconf = configuration_data()
testconf.set('MUTTER_TYPELIB_DIR', mutter_typelibdir)
testconf.set('srcdir', meson.current_source_dir())
configure_file(
  input: 'run-test.sh.in',
  output: 'run-test.sh',
  configuration: testconf
)
