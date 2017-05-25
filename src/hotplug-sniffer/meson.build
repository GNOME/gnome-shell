hotplug_sources = [
  'hotplug-mimetypes.h',
  'shell-mime-sniffer.h',
  'shell-mime-sniffer.c',
  'hotplug-sniffer.c'
]

executable('gnome-shell-hotplug-sniffer', hotplug_sources,
  dependencies: [gio_dep, gdk_pixbuf_dep],
  include_directories: include_directories('../..'),
  install_dir: libexecdir,
  install: true
)

service_file = 'org.gnome.Shell.HotplugSniffer.service'

configure_file(
  input: service_file + '.in',
  output: service_file,
  configuration: service_data,
  install_dir: servicedir
)
