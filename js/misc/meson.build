jsconf = configuration_data()
jsconf.set('PACKAGE_NAME', meson.project_name())
jsconf.set('PACKAGE_VERSION', meson.project_version())
jsconf.set('GETTEXT_PACKAGE', meson.project_name())
jsconf.set('LIBMUTTER_API_VERSION', mutter_api_version)
jsconf.set10('HAVE_NETWORKMANAGER', have_networkmanager)
jsconf.set10('HAVE_SOUP2', have_soup2)
jsconf.set('datadir', datadir)
jsconf.set('libexecdir', libexecdir)

config_js = configure_file(
  input: 'config.js.in',
  output: 'config.js',
  configuration: jsconf
)
