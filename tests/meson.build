testconf = configuration_data()
testconf.set('MUTTER_TYPELIB_DIR', mutter_typelibdir)
testconf.set('srcdir', meson.current_source_dir())
run_test = configure_file(
  input: 'run-test.sh.in',
  output: 'run-test.sh',
  configuration: testconf
)

testenv = environment()
testenv.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.build_root(), 'data'))

foreach test : ['insertSorted', 'jsParse', 'markup', 'params', 'url']
  test(test, run_test,
    args: 'unit/@0@.js'.format(test),
    env: testenv,
    workdir: meson.current_source_dir())
endforeach

run_gnome_shell = find_program('run-gnome-shell.sh')
test('run-gnome-shell-wayland', run_gnome_shell,
  args: '--wayland',
  is_parallel: false)
test('run-gnome-shell-x11', run_gnome_shell,
  args: '--x11',
  is_parallel: false)
