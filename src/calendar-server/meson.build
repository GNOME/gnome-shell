calendar_sources = [
  'gnome-shell-calendar-server.c',
  'calendar-debug.h',
  'calendar-sources.c',
  'calendar-sources.h',
  'reminder-watcher.c',
  'reminder-watcher.h'
]

calendar_server = executable('gnome-shell-calendar-server', calendar_sources,
  dependencies: [ecal_dep, eds_dep, gio_dep],
  include_directories: include_directories('..', '../..'),
  c_args: [
    '-DPREFIX="@0@"'.format(prefix),
    '-DLIBDIR="@0@"'.format(libdir),
    '-DDATADIR="@0@"'.format(datadir),
    '-DG_LOG_DOMAIN="ShellCalendarServer"'
  ],
  install_dir: libexecdir,
  install: true
)

service_file = 'org.gnome.Shell.CalendarServer.service'

configure_file(
  input: service_file + '.in',
  output: service_file,
  configuration: service_data,
  install_dir: servicedir
)

i18n.merge_file('evolution-calendar.desktop',
  input: 'evolution-calendar.desktop.in',
  output: 'evolution-calendar.desktop',
  po_dir: po_dir,
  install: true,
  install_dir: desktopdir,
  type: 'desktop'
)
