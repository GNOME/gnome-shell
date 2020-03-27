launcherconf = configuration_data()
launcherconf.set('app_id', app_id)
launcherconf.set('PACKAGE_NAME', package_name)
launcherconf.set('PACKAGE_VERSION', meson.project_version())
launcherconf.set('prefix', prefix)
launcherconf.set('libdir', libdir)
launcherconf.set('pkgdatadir', pkgdatadir)
launcherconf.set('gjs', gjs.path())

configure_file(
  input: prgname + '.in',
  output: prgname,
  configuration: launcherconf,
  install_dir: bindir,
  install_mode: 'rwxr-xr-x',
)

configure_file(
  input: app_id + '.in',
  output: app_id,
  configuration: launcherconf,
  install_dir: pkgdatadir,
)

gnome.compile_resources(
  app_id + '.src',
  app_id + '.src.gresource.xml',
  source_dir: ['.', '../../../js'],
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir
)
