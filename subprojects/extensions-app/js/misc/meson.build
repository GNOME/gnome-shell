config_js = configure_file(
  input: 'config.js.in',
  output: '@BASENAME@',
  configuration: {
    'GETTEXT_VERSION': meson.project_version(),
  }
)
