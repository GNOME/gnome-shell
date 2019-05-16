launcherconf = configuration_data()
launcherconf.set('PACKAGE_NAME', meson.project_name())
launcherconf.set('prefix', prefix)
launcherconf.set('libdir', libdir)

dbus_services = [
  'org.gnome.Shell.Notifications',
]

foreach service : dbus_services
  configure_file(
    input: 'dbus-service.in',
    output: service,
    configuration: launcherconf,
    install_dir: pkgdatadir,
  )

  serviceconf = configuration_data()
  serviceconf.set('service', service)
  serviceconf.set('gjs', gjs.path())
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
    gresource_bundle: true,
    install: true,
    install_dir: pkgdatadir
  )
endforeach
