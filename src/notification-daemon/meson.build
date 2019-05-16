notification_daemon_sources = [
  'nd-daemon.c',
  'main.c'
]

notificationsXML = join_paths(
    meson.source_root(),
    'data',
    'dbus-interfaces',
    'org.freedesktop.Notifications.xml'
)

notification_daemon_sources += gnome.gdbus_codegen('nd-notifications',
  notificationsXML,
  namespace: 'Nd',
  interface_prefix: 'org.freedesktop'
)

executable('gnome-shell-notification-daemon',
  notification_daemon_sources,
  dependencies: gio_unix_dep,
  install_dir: libexecdir,
  install: true
)
