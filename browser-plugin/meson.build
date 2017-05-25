plugin_sources = [
  'browser-plugin.c',
  'npapi/npapi.h',
  'npapi/npfunctions.h',
  'npapi/npruntime.h',
  'npapi/nptypes.h'
]

shared_module('gnome-shell-browser-plugin', plugin_sources,
  dependencies: [gio_dep, json_glib_dep],
  c_args: ['-DG_LOG_DOMAIN="GnomeShellBrowserPlugin"'],
  # Browsers can unload and reload the module while browsing, which is not
  # supported by GObject.
  # We pass -Wl,-z,nodelete to the linker to ensure the module is never
  # unloaded. See https://bugzilla.gnome.org/show_bug.cgi?id=737932.
  link_args: ['-Wl,-z,nodelete'],
  install: true,
  install_dir: plugindir
)
