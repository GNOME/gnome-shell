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
    workdir: meson.current_source_dir())
endforeach

perf_tests = [
  {
    'name': 'basic',
  },
  {
    'name': 'closeWithActiveWindows',
  },
  {
    'name': 'headlessStart',
    'options': ['--hotplug'],
  },
]

gvc_typelib_path = fs.parent(libgvc.get_variable('libgvc_gir')[1].full_path())
libgvc_path = fs.parent(libgvc.get_variable('libgvc').full_path())

background_file = files(join_paths('data', 'background.png'))

perf_testenv = environment()
perf_testenv.set('G_DEBUG', 'fatal-warnings')
perf_testenv.set('G_MESSAGES_DEBUG', 'GNOME Shell')
perf_testenv.set('GNOME_SHELL_DATADIR', data_builddir)
perf_testenv.set('GNOME_SHELL_BUILDDIR', src_builddir)
perf_testenv.set('GNOME_SHELL_SESSION_MODE', 'user')
perf_testenv.set('SHELL_BACKGROUND_IMAGE', '@0@'.format(background_file))
perf_testenv.append('GI_TYPELIB_PATH', gvc_typelib_path, separator: ':')
perf_testenv.append('LD_LIBRARY_PATH', libgvc_path, separator: ':')

foreach perf_test : perf_tests
  test_name = perf_test['name']
  options = perf_test.get('options', [])

  test('perf-' + test_name, dbus_runner,
    args: [
      perf_tool,
      '--headless',
      '--perf=@0@'.format(test_name),
      options,
    ],
    is_parallel: false,
    env: perf_testenv,
  )
endforeach
