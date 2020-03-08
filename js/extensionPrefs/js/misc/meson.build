jsconf = configuration_data()
jsconf.set('PACKAGE_VERSION', meson.project_version())

extensions_config = configure_file(
  input: 'config.js.in',
  output: 'config.js',
  configuration: jsconf
)
