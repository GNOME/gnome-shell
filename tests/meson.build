testconf = configuration_data()
testconf.set('MUTTER_TYPELIB_DIR', mutter_typelibdir)
testconf.set('srcdir', meson.current_source_dir())
run_test = configure_file(
  input: 'run-test.sh.in',
  output: 'run-test.sh',
  configuration: testconf
)

mutter_tests_datadir = mutter_test_dep.get_variable('tests_datadir')
dbusrunnerconf = configuration_data()
dbusrunnerconf.set('MUTTER_TEST_PKGDATADIR', mutter_tests_datadir)
dbusrunnerconf.set('SRCDIR', meson.current_source_dir())

dbus_runner = configure_file(
  input: 'gnome-shell-dbus-runner.py.in',
  output: 'gnome-shell-dbus-runner.py',
  configuration: dbusrunnerconf,
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

perf_tests = [
  'basic',
]

gvc_typelib_path = fs.parent(libgvc.get_variable('libgvc_gir')[1].full_path())
libgvc_path = fs.parent(libgvc.get_variable('libgvc').full_path())

background_file = files(join_paths('data', 'background.png'))

perf_testenv = testenv
perf_testenv.set('G_DEBUG', 'fatal-criticals')
perf_testenv.set('G_MESSAGES_DEBUG', 'GNOME Shell')
perf_testenv.set('GNOME_SHELL_DATADIR', data_builddir)
perf_testenv.set('GNOME_SHELL_BUILDDIR', src_builddir)
perf_testenv.set('SHELL_BACKGROUND_IMAGE', '@0@'.format(background_file))
perf_testenv.append('GI_TYPELIB_PATH', gvc_typelib_path, separator: ':')
perf_testenv.append('LD_LIBRARY_PATH', libgvc_path, separator: ':')

foreach perf_test : perf_tests
  test('perf-' + perf_test, dbus_runner,
    args: [
      perf_tool,
      '--headless',
      '--perf=@0@'.format(perf_test),
    ],
    env: perf_testenv,
  )
endforeach
