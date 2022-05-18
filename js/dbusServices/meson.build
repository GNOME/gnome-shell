launcherconf = configuration_data()
launcherconf.set('PACKAGE_NAME', meson.project_name())
launcherconf.set('prefix', prefix)
launcherconf.set('libdir', libdir)

dbus_services = {
  'org.gnome.Shell.Extensions': 'extensions',
  'org.gnome.Shell.Notifications': 'notifications',
  'org.gnome.Shell.Screencast': 'screencast',
  'org.gnome.ScreenSaver': 'screensaver',
}

config_dir = '@0@/..'.format(meson.current_build_dir())

foreach service, dir : dbus_services
  configure_file(
    input: 'dbus-service.in',
    output: service,
    configuration: launcherconf,
    install_dir: pkgdatadir,
  )

  serviceconf = configuration_data()
  serviceconf.set('service', service)
  serviceconf.set('gjs', gjs.full_path())
  serviceconf.set('pkgdatadir', pkgdatadir)

  configure_file(
    input: 'dbus-service.service.in',
    output: service + '.service',
    configuration: serviceconf,
    install_dir: servicedir
  )

  gnome.compile_resources(
    service + '.src',
    service + '.src.gresource.xml',
    dependencies: [config_js],
    source_dir: ['.', '..', dir, config_dir],
    gresource_bundle: true,
    install: true,
    install_dir: pkgdatadir
  )
endforeach
