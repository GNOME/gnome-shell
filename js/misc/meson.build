jsconf = configuration_data()
jsconf.set('PACKAGE_NAME', meson.project_name())
jsconf.set('PACKAGE_VERSION', meson.project_version())
jsconf.set('GETTEXT_PACKAGE', meson.project_name())
jsconf.set('LIBMUTTER_API_VERSION', mutter_api_version)
jsconf.set10('HAVE_NETWORKMANAGER', have_networkmanager)
jsconf.set10('HAVE_PORTAL_HELPER', have_portal_helper)
jsconf.set('UTILS_FOLDER_APPS', run_command(
  generate_app_list, '../../data/default-apps/utilities-folder.txt',
  check: true,
).stdout())
jsconf.set('SYSTEM_FOLDER_APPS', run_command(
  generate_app_list, '../../data/default-apps/system-folder.txt',
  check: true,
).stdout())
jsconf.set('datadir', datadir)
jsconf.set('libexecdir', libexecdir)

config_js = configure_file(
  input: 'config.js.in',
  output: 'config.js',
  configuration: jsconf
)
