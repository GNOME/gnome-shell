subdir('misc')

js_resources = gnome.compile_resources(
  'js-resources', 'js-resources.gresource.xml',
  source_dir: ['.', meson.current_build_dir()],
  c_name: 'shell_js_resources',
  dependencies: [config_js]
)
