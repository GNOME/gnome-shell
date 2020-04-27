service_data = configuration_data()
service_data.set('libexecdir', libexecdir)

subdir('calendar-server')
subdir('hotplug-sniffer')
subdir('st')
subdir('tray')

script_data = configuration_data()
script_data.set('bindir', bindir)
script_data.set('datadir', datadir)
script_data.set('libdir', libdir)
script_data.set('libexecdir', libexecdir)
script_data.set('pkgdatadir', pkgdatadir)
script_data.set('pkglibdir', pkglibdir)
script_data.set('PYTHON', python.path())
script_data.set('VERSION', meson.project_version())

script_tools = ['gnome-shell-perf-tool']

if get_option('extensions_tool')
  script_tools += 'gnome-shell-extension-tool'
endif

foreach tool : script_tools
  configure_file(
    input: tool + '.in',
    output: tool,
    configuration: script_data,
    install_dir: bindir
  )
endforeach

install_data('gnome-shell-extension-prefs',
  install_dir: bindir
)

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
  gtk_dep,
  atk_bridge_dep,
  gjs_dep,
  gdk_x11_dep,
  clutter_dep,
  cogl_pango_dep,
  startup_dep,
  gi_dep,
  polkit_dep,
  gcr_dep,
  libsystemd_dep
]

gnome_shell_deps += nm_deps
gnome_shell_deps += recorder_deps

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
  dependencies: [gio_dep, clutter_dep],
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
  'shell-embedded-window.h',
  'shell-glsl-effect.h',
  'shell-gtk-embed.h',
  'shell-global.h',
  'shell-invert-lightness-effect.h',
  'shell-action-modes.h',
  'shell-mount-operation.h',
  'shell-perf-log.h',
  'shell-screenshot.h',
  'shell-stack.h',
  'shell-tray-icon.h',
  'shell-tray-manager.h',
  'shell-util.h',
  'shell-window-tracker.h',
  'shell-wm.h'
]

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

libshell_sources = [
  'gnome-shell-plugin.c',
  'shell-app.c',
  'shell-app-system.c',
  'shell-app-usage.c',
  'shell-blur-effect.c',
  'shell-embedded-window.c',
  'shell-embedded-window-private.h',
  'shell-global.c',
  'shell-glsl-effect.c',
  'shell-gtk-embed.c',
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
  'shell-stack.c',
  'shell-tray-icon.c',
  'shell-tray-manager.c',
  'shell-util.c',
  'shell-window-tracker.c',
  'shell-wm.c'
]

if have_networkmanager
  libshell_sources += 'shell-network-agent.c'
endif

libshell_private_sources = [
  'shell-app-cache.c',
]

if enable_recorder
    libshell_sources += ['shell-recorder.c']
    libshell_public_headers += ['shell-recorder.h']

    libshell_private_sources += ['shell-recorder-src.c']
    libshell_private_headers += ['shell-recorder-src.h']
endif


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

libshell = library('gnome-shell',
  sources: libshell_gir_sources + libshell_no_gir_sources,
  dependencies: gnome_shell_deps + [libshell_menu_dep, libst_dep, mutter_dep, gnome_desktop_dep, m_dep],
  include_directories: [conf_inc, st_inc, include_directories('tray')],
  c_args: gnome_shell_cflags,
  link_with: [libtray],
  build_rpath: mutter_typelibdir,
  install_rpath: install_rpath,
  install_dir: pkglibdir,
  install: true
)

libshell_dep = declare_dependency(link_with: libshell)

libshell_gir_includes = [
  'Clutter-@0@'.format(mutter_api_version),
  'ClutterX11-@0@'.format(mutter_api_version),
  'Meta-@0@'.format(mutter_api_version),
  'Gcr-3',
  'PolkitAgent-1.0'
]

if have_networkmanager
  libshell_gir_includes += ['NM-1.0']
endif

libshell_gir_includes += [
  libgvc_gir[0],
  libst_gir[0]
]

gnome.generate_gir(libshell,
  sources: libshell_gir_sources,
  nsversion: '0.1',
  namespace: 'Shell',
  includes: libshell_gir_includes,
  extra_args: ['--quiet'],
  install_dir_gir: pkgdatadir,
  install_dir_typelib: pkglibdir,
  install: true
)

executable('gnome-shell', 'main.c',
  c_args: gnome_shell_cflags + [
    '-DMUTTER_TYPELIB_DIR="@0@"'.format(mutter_typelibdir)
  ],
  dependencies: gnome_shell_deps + [libshell_dep, libst_dep, mutter_dep],
  include_directories: [conf_inc, st_inc, include_directories('tray')],
  build_rpath: mutter_typelibdir,
  install_rpath: install_rpath,
  install: true
)

if have_networkmanager
  executable('gnome-shell-portal-helper',
    'gnome-shell-portal-helper.c', portal_resources,
    c_args: tools_cflags,
    dependencies: tools_deps,
    include_directories: [conf_inc],
    install_dir: libexecdir,
    install: true
  )
endif

executable('gnome-shell-perf-helper', 'shell-perf-helper.c',
  dependencies: [gtk_dep, gio_dep, m_dep],
  include_directories: [conf_inc],
  install_dir: libexecdir,
  install: true
)

executable('run-js-test', 'run-js-test.c',
  dependencies: [mutter_dep, gio_dep, gi_dep, gjs_dep],
  include_directories: [conf_inc],
  link_with: libshell,
  build_rpath: mutter_typelibdir,
)
