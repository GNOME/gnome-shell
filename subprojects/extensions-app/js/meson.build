launcherconf = configuration_data()
launcherconf.set('app_id', app_id)
launcherconf.set('PACKAGE_NAME', package_name)
if vcs_tag != ''
  launcherconf.set('PACKAGE_VERSION', '@0@ (@1@)'.format(package_version, vcs_tag))
else
  launcherconf.set('PACKAGE_VERSION', package_version)
endif
launcherconf.set('prefix', prefix)
launcherconf.set('libdir', libdir)
launcherconf.set('pkgdatadir', pkgdatadir)
launcherconf.set('gjs', gjs.full_path())

configure_file(
  input: prgname + '.in',
  output: prgname,
  configuration: launcherconf,
  install_dir: bindir,
  install_mode: 'rwxr-xr-x',
)

configure_file(
  input: base_id + '.in',
  output: app_id,
  configuration: launcherconf,
  install_dir: pkgdatadir,
)

gnome.compile_resources(
  app_id + '.src',
  configure_file(
    input: base_id + '.src.gresource.xml.in',
    output: app_id + '.src.gresource.xml',
    configuration: {'profile': '/'.join(profile.split('.')) },
  ),
  source_dir: ['.', '../../../js'],
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir
)
