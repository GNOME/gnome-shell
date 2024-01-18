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

unit_tests = [
    'highlighter',
    'injectionManager',
    'insertSorted',
    'jsParse',
    'markup',
    'params',
    'signalTracker',
    'url',
    'versionCompare',
]

foreach test : unit_tests
  test(test, run_test,
    args: 'unit/@0@.js'.format(test),
    suite: 'unit',
    workdir: meson.current_source_dir())
endforeach

shell_tests = [
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
  {
    'name': 'fittsy',
  },
]

gvc_typelib_path = fs.parent(libgvc.get_variable('libgvc_gir')[1].full_path())
libgvc_path = fs.parent(libgvc.get_variable('libgvc').full_path())

background_file = join_paths(meson.current_source_dir(), 'data', 'background.png')

shell_testenv = environment()
shell_testenv.set('G_DEBUG', 'fatal-warnings')
shell_testenv.set('G_MESSAGES_DEBUG', 'GNOME Shell')
shell_testenv.set('GNOME_SHELL_DATADIR', data_builddir)
shell_testenv.set('GNOME_SHELL_BUILDDIR', src_builddir)
shell_testenv.set('GNOME_SHELL_SESSION_MODE', 'user')
shell_testenv.set('SHELL_BACKGROUND_IMAGE', '@0@'.format(background_file))
shell_testenv.append('GI_TYPELIB_PATH', gvc_typelib_path, separator: ':')
shell_testenv.append('LD_LIBRARY_PATH', libgvc_path, separator: ':')

foreach shell_test : shell_tests
  test_name = shell_test['name']
  options = shell_test.get('options', [])

  test(test_name, dbus_runner,
    suite: 'shell',
    args: [
      test_tool,
      '--headless',
      options,
      '@0@/shell/@1@.js'.format(meson.current_source_dir(), test_name),
    ],
    is_parallel: false,
    env: shell_testenv,
  )
endforeach
