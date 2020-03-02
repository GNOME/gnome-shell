tray_sources = [
  'na-tray-child.c',
  'na-tray-child.h',
  'na-tray-manager.c',
  'na-tray-manager.h'
]

libtray = static_library('tray', tray_sources,
  c_args: ['-DG_LOG_DOMAIN="notification_area"'],
  dependencies: [clutter_dep, gtk_dep],
  include_directories: conf_inc
)
