src_builddir = meson.current_build_dir()

service_data = configuration_data()
service_data.set('libexecdir', libexecdir)

introspection_common = {
  'install_gir': false,
  'install_typelib': true,
  'install_dir_typelib': pkglibdir,
  'fatal_warnings': get_option('werror'),
}

subdir('calendar-server')
subdir('hotplug-sniffer')
subdir('st')
if have_x11_client
  subdir('tray')
endif

script_data = configuration_data()
script_data.set('bindir', bindir)
script_data.set('datadir', datadir)
script_data.set('libdir', libdir)
script_data.set('libexecdir', libexecdir)
script_data.set('pkgdatadir', pkgdatadir)
script_data.set('pkglibdir', pkglibdir)
script_data.set('PYTHON', python.full_path())
script_data.set('VERSION', meson.project_version())

test_tool = configure_file(
  input: 'gnome-shell-test-tool.in',
  output: 'gnome-shell-test-tool',
  configuration: script_data,
  install_dir: bindir
)

if get_option('extensions_tool')
  configure_file(
    input: 'gnome-shell-extension-tool.in',
    output: 'gnome-shell-extension-tool',
    configuration: script_data,
    install_dir: bindir
  )
endif

typelib_cflags = [
  '-DMUTTER_TYPELIB_DIR="@0@"'.format(mutter_typelibdir),
  '-DSHELL_TYPELIB_DIR="@0@"'.format(pkglibdir),
]

gnome_shell_cflags = [
  '-DCLUTTER_ENABLE_EXPERIMENTAL_API',
  '-DCOGL_ENABLE_EXPERIMENTAL_API',
  '-DVERSION="@0@"'.format(meson.project_version()),
  '-DLOCALEDIR="@0@"'.format(localedir),
  '-DDATADIR="@0@"'.format(datadir),
  '-DGNOME_SHELL_LIBEXECDIR="@0@"'.format(libexecdir),
  '-DGNOME_SHELL_DATADIR="@0@"'.format(pkgdatadir),
  '-DGNOME_SHELL_PKGLIBDIR="@0@"'.format(pkglibdir)
]

install_rpath = ':'.join([mutter_typelibdir, pkglibdir])

gnome_shell_deps = [
  gio_unix_dep,
  libxml_dep,
  atk_bridge_dep,
  gdk_pixbuf_dep,
  gjs_dep,
  mtk_dep,
  clutter_dep,
  gi_dep,
  polkit_dep,
  gcr_dep,
  libsystemd_dep,
  libpipewire_dep,
  pango_dep,
]

if have_x11_client
  gnome_shell_deps += x11_dep
endif

gnome_shell_deps += nm_deps

tools_cflags = '-DLOCALEDIR="@0@"'.format(localedir)
tools_deps = [gio_dep, gjs_dep]

libshell_menu_sources = [
  'gtkactionmuxer.h',
  'gtkactionmuxer.c',
  'gtkactionobservable.h',
  'gtkactionobservable.c',
  'gtkactionobserver.h',
  'gtkactionobserver.c'
]

libshell_menu = library('gnome-shell-menu',
  sources: libshell_menu_sources,
  dependencies: [gio_dep, clutter_dep, mtk_dep],
  include_directories: conf_inc,
  build_rpath: mutter_typelibdir,
  install_rpath: mutter_typelibdir,
  install_dir: pkglibdir,
  install: true
)

libshell_menu_dep = declare_dependency(link_with: libshell_menu)

libshell_public_headers = [
  'shell-app.h',
  'shell-app-system.h',
  'shell-app-usage.h',
  'shell-blur-effect.h',
  'shell-camera-monitor.h',
  'shell-edge-drag-gesture.h',
  'shell-glsl-effect.h',
  'shell-global.h',
  'shell-invert-lightness-effect.h',
  'shell-action-modes.h',
  'shell-mount-operation.h',
  'shell-perf-log.h',
  'shell-screenshot.h',
  'shell-square-bin.h',
  'shell-stack.h',
  'shell-time-change-source.h',
  'shell-util.h',
  'shell-window-preview.h',
  'shell-window-preview-layout.h',
  'shell-window-tracker.h',
  'shell-wm.h',
  'shell-workspace-background.h'
]

if have_x11_client
  libshell_public_headers += [
    'shell-tray-icon.h',
    'shell-tray-manager.h',
  ]
endif

if have_networkmanager
  libshell_public_headers += 'shell-network-agent.h'
endif

libshell_private_headers = [
  'shell-app-private.h',
  'shell-app-cache-private.h',
  'shell-app-system-private.h',
  'shell-global-private.h',
  'shell-window-tracker-private.h',
  'shell-wm-private.h'
]

if have_x11_client
  libshell_private_headers += [
    'shell-tray-icon-private.h',
  ]
endif

libshell_sources = [
  'gnome-shell-plugin.c',
  'shell-app.c',
  'shell-app-system.c',
  'shell-app-usage.c',
  'shell-blur-effect.c',
  'shell-camera-monitor.c',
  'shell-edge-drag-gesture.c',
  'shell-global.c',
  'shell-glsl-effect.c',
  'shell-invert-lightness-effect.c',
  'shell-keyring-prompt.c',
  'shell-keyring-prompt.h',
  'shell-mount-operation.c',
  'shell-perf-log.c',
  'shell-polkit-authentication-agent.c',
  'shell-polkit-authentication-agent.h',
  'shell-screenshot.c',
  'shell-secure-text-buffer.c',
  'shell-secure-text-buffer.h',
  'shell-square-bin.c',
  'shell-stack.c',
  'shell-time-change-source.c',
  'shell-util.c',
  'shell-window-preview.c',
  'shell-window-preview-layout.c',
  'shell-window-tracker.c',
  'shell-wm.c',
  'shell-workspace-background.c'
]

if have_x11_client
  libshell_sources += [
    'shell-tray-icon.c',
    'shell-tray-manager.c',
  ]
endif

if have_networkmanager
  libshell_sources += 'shell-network-agent.c'
endif

libshell_private_sources = [
  'shell-app-cache.c',
]

libshell_enums = gnome.mkenums_simple('shell-enum-types',
  sources: libshell_public_headers
)

libshell_gir_sources = [
  libshell_enums,
  libshell_public_headers,
  libshell_sources
]

libshell_no_gir_sources = [
  js_resources,
  libshell_private_headers,
  libshell_private_sources
]

dbus_generated = gnome.gdbus_codegen('org-gtk-application',
  'org.gtk.Application.xml',
  namespace: 'Shell'
)

dbus_generated += gnome.gdbus_codegen('switcheroo-control',
  '../data/dbus-interfaces/net.hadess.SwitcherooControl.xml',
  namespace: 'Shell'
)

libshell_no_gir_sources += dbus_generated

libshell_link_with = []
if have_x11_client
  libshell_link_with += [libtray]
endif

libshell = library('shell-' + mutter_api_version,
  sources: libshell_gir_sources + libshell_no_gir_sources,
  dependencies: gnome_shell_deps + [libshell_menu_dep, libst_dep, mutter_dep, gnome_desktop_dep, m_dep],
  include_directories: [conf_inc, include_directories('tray')],
  c_args: gnome_shell_cflags,
  link_with: libshell_link_with,
  build_rpath: mutter_typelibdir,
  install_rpath: install_rpath,
  install_dir: pkglibdir,
  install: true
)

libshell_dep = declare_dependency(
  link_with: libshell,
  dependencies: [libst_dep, mutter_dep],
  include_directories: [include_directories('tray')],
)

libshell_gir_includes = [
  'Clutter-@0@'.format(mutter_api_version),
  'Meta-@0@'.format(mutter_api_version),
  'Gcr-4',
  'GioUnix-2.0',
  'PolkitAgent-1.0',
  'GdkPixbuf-2.0'
]

if have_networkmanager
  libshell_gir_includes += ['NM-1.0']
endif

libshell_gir_includes += [
  libgvc_gir[0],
  libst_gir[0]
]

libshell_gir = gnome.generate_gir(libshell,
  sources: libshell_gir_sources,
  nsversion: mutter_api_version,
  namespace: 'Shell',
  includes: libshell_gir_includes,
  extra_args: ['--quiet'],
  kwargs: introspection_common,
)

gnome_shell_binary = executable('gnome-shell', 'main.c',
  c_args: gnome_shell_cflags + typelib_cflags,
  dependencies: gnome_shell_deps + [libshell_dep, libst_dep, mutter_dep],
  include_directories: [conf_inc],
  build_rpath: mutter_typelibdir,
  install_rpath: install_rpath,
  install: true
)

if have_portal_helper
  executable('gnome-shell-portal-helper',
    'gnome-shell-portal-helper.c', portal_resources,
    c_args: tools_cflags,
    dependencies: tools_deps,
    include_directories: [conf_inc],
    install_dir: libexecdir,
    install: true
  )
endif

gnome_shell_perf_helper = executable('gnome-shell-perf-helper', 'shell-perf-helper.c', js_resources,
  dependencies: [gtk_dep, gio_dep, m_dep],
  include_directories: [conf_inc],
  install_dir: libexecdir,
  install: true
)

run_test = executable('run-js-test', 'run-js-test.c',
  dependencies: [gio_dep, gi_dep, gjs_dep, libshell_dep],
  c_args: typelib_cflags,
  include_directories: [conf_inc],
  build_rpath: mutter_typelibdir,
)
