data_builddir = meson.current_build_dir()

desktop_files = [
  'org.gnome.Shell.Extensions.desktop',
]
service_files = []

if have_portal_helper
  desktop_files += 'org.gnome.Shell.PortalHelper.desktop'
  service_files += 'org.gnome.Shell.PortalHelper.service'
endif

foreach desktop_file : desktop_files
  i18n.merge_file(
    input: desktop_file + '.in',
    output: desktop_file,
    po_dir: po_dir,
    install: true,
    install_dir: desktopdir,
    type: 'desktop'
  )
endforeach

desktop_directories = [
  'X-GNOME-Shell-Utilities.directory',
  'X-GNOME-Shell-System.directory',
]
foreach desktop_directory : desktop_directories
  i18n.merge_file(
    input: f'@desktop_directory@.desktop.in',
    output: desktop_directory,
    po_dir: po_dir,
    install: true,
    install_dir: join_paths(datadir, 'desktop-directories'),
    type: 'desktop',
  )
endforeach

serviceconf = configuration_data()
serviceconf.set('libexecdir', libexecdir)
foreach service_file : service_files
  configure_file(
    input: service_file + '.in',
    output: service_file,
    configuration: serviceconf,
    install_dir: servicedir
  )
endforeach

theme_deps = []

subdir('dbus-interfaces')
subdir('icons')
subdir('theme')

data_resources = [
  {'name': 'dbus-interfaces'},
  {'name': 'icons'},
  {'name': 'osk-layouts'},
  {'name': 'theme', 'deps': theme_deps}
]
data_resources_targets = []
foreach resource : data_resources
  name = resource.get('name')
  deps = resource.get('deps', [])

  data_resources_targets += gnome.compile_resources(
    'gnome-shell-' + name,
    'gnome-shell-@0@.gresource.xml'.format(name),
    source_dir: name,
    dependencies: deps,
    gresource_bundle: true,
    install: true,
    install_dir: pkgdatadir
  )
endforeach

perfconf = configuration_data()
perfconf.set('datadir', datadir)
configure_file(
  input: 'perf-background.xml.in',
  output: 'perf-background.xml',
  configuration: perfconf,
  install_dir: pkgdatadir
)

keybinding_files = [
  '50-gnome-shell-launchers.xml',
  '50-gnome-shell-screenshots.xml',
  '50-gnome-shell-system.xml',
]

install_data(keybinding_files, install_dir: keysdir)


schemaconf = configuration_data()
schemaconf.set('GETTEXT_PACKAGE', meson.project_name())
schemaconf.set('DASH_APPS', run_command(
  generate_app_list, 'default-apps/dash.txt',
  check: true,
).stdout())
schemaconf.set('APP_GRID_APPS', run_command(
  generate_app_list, '--pages', 'default-apps/app-grid.txt',
  check: true,
).stdout())
schema = configure_file(
  input: 'org.gnome.shell.gschema.xml.in',
  output: 'org.gnome.shell.gschema.xml',
  configuration: schemaconf,
  install_dir: schemadir
)
install_data('00_org.gnome.shell.gschema.override', install_dir: schemadir)

if have_systemd
  unitconf = configuration_data()
  unitconf.set('bindir', bindir)
  supported_sessions = ['wayland']
  if have_x11
    configure_file(
      input: 'org.gnome.Shell@x11.service.in',
      output: 'org.gnome.Shell@x11.service',
      configuration: unitconf,
      install_dir: systemduserunitdir
    )
    supported_sessions += ['x11']
  endif

  configure_file(
    input: 'org.gnome.Shell@wayland.service.in',
    output: 'org.gnome.Shell@wayland.service',
    configuration: unitconf,
    install_dir: systemduserunitdir
  )


  units = files('org.gnome.Shell-disable-extensions.service')
  targetconf = configuration_data()
  sessions_wants = []
  foreach session: supported_sessions
    sessions_wants += 'org.gnome.Shell@' + session + '.service'
  endforeach
  targetconf.set('SUPPORTED_SESSIONS', ' '.join(sessions_wants))
  target = configure_file(
    input: 'org.gnome.Shell.target.in',
    output: 'org.gnome.Shell.target',
    configuration: targetconf
  )
  units += target

  install_data(units, install_dir: systemduserunitdir)
endif

# for unit tests - gnome.compile_schemas() only looks in srcdir
compiled_schemas = custom_target('compile-schemas',
  input: schema,
  output: 'gschemas.compiled',
  command: [find_program('glib-compile-schemas'), '--strict', data_builddir],
)
