testconf = configuration_data()
testconf.set('MUTTER_TYPELIB_DIR', mutter_typelibdir)
testconf.set('srcdir', meson.current_source_dir())
run_test = configure_file(
  input: 'run-test.sh.in',
  output: 'run-test.sh',
  configuration: testconf
)

testenv = environment()
testenv.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.project_build_root(), 'data'))

tests = [
    'highlighter',
    'insertSorted',
    'jsParse',
    'markup',
    'params',
    'signalTracker',
    'url',
    'versionCompare',
]

foreach test : tests
  test(test, run_test,
    args: 'unit/@0@.js'.format(test),
    env: testenv,
    workdir: meson.current_source_dir())
endforeach
