shew_public_headers = files(
  'shew-external-window.h',
  'shew-window-exporter.h',
)

shew_sources = [
  'shew-external-window-wayland.c',
  'shew-external-window-x11.c',
  'shew-external-window.c',
  'shew-window-exporter.c',
]

libshew = library(full_name,
  sources: shew_sources,
  dependencies: [gtk_dep, x11_dep],
  install_dir: pkglibdir,
  install: true,
)

libshew_gir = gnome.generate_gir(libshew,
  sources: shew_sources + shew_public_headers,
  nsversion: api_version,
  namespace: 'Shew',
  includes: ['Gdk-4.0', 'Gtk-4.0'],
  extra_args: ['--quiet'],
  install_dir_gir: girdir,
  install_dir_typelib: typelibdir,
  install: true,
)
