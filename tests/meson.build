mutter_tests_datadir = mutter_test_dep.get_variable('tests_datadir')
dbusrunnerconf = configuration_data()
dbusrunnerconf.set('MUTTER_TEST_PKGDATADIR', mutter_tests_datadir)
dbusrunnerconf.set('SRCDIR', meson.current_source_dir())

dbus_runner = configure_file(
  input: 'gnome-shell-dbus-runner.py.in',
  output: 'gnome-shell-dbus-runner.py',
  configuration: dbusrunnerconf,
)

gvc_typelib_path = fs.parent(libgvc.get_variable('libgvc_gir')[1].full_path())
shell_typelib_path = fs.parent(libshell_gir[1].full_path())
st_typelib_path = fs.parent(libst_gir[1].full_path())

# Shared tests dependencies.
tests_dependencies = [
  compiled_schemas,
  data_resources_targets,
  libst_gir[1],
  libshell_gir[1],
  libgvc_gir[1],
]

unit_testenv = environment()
unit_testenv.set('GNOME_SHELL_DATADIR', data_builddir)
unit_testenv.append('GI_TYPELIB_PATH', gvc_typelib_path, separator: ':')
unit_testenv.append('GI_TYPELIB_PATH', shell_typelib_path, separator: ':')
unit_testenv.append('GI_TYPELIB_PATH', st_typelib_path, separator: ':')

unit_tests = {
  'breakManager': {},
  'extensionUtils': {},
  'fileUtils': {
    'env': {
      'XDG_DATA_DIRS': join_paths(meson.current_source_dir(), 'unit/fixtures/datadirs/system'),
      'XDG_DATA_HOME': join_paths(meson.current_source_dir(), 'unit/fixtures/datadirs/home'),
    },
  },
  'highlighter': {},
  'injectionManager': {},
  'insertSorted': {},
  'jsParse': {},
  'markup': {},
  'params': {},
  'signalTracker': {},
  'timeLimitsManager': {
    # The time limits tests hard-code UTC to keep the code brief, so must be
    # run in UTC
    'env': { 'TZ': 'UTC' },
  },
  'url': {},
  'versionCompare': {},
}

foreach test, extra_args : unit_tests
  local_test_env = unit_testenv
  foreach var, value : extra_args.get('env', {})
    local_test_env.set(var, value)
  endforeach

  test(test, jasmine,
    args: [
      '--module',
      '--tap',
      '--no-config',
      '--interpreter', run_test,
      'unit/@0@.js'.format(test),
    ],
    suite: 'unit',
    depends: tests_dependencies,
    env: local_test_env,
    protocol: 'tap',
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
    depends: [
      gnome_shell_binary,
      gnome_shell_perf_helper,
      tests_dependencies,
    ],
    is_parallel: false,
    env: shell_testenv,
  )
endforeach
