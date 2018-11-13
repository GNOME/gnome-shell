project('gnome-shell', 'c',
  version: '3.30.2',
  meson_version: '>= 0.47.0',
  license: 'GPLv2+'
)

# We depend on a specific version of the libmutter API. The mutter variants of
# the Cogl and Clutter libraries also use this API version.
mutter_api_version = '3'

clutter_pc = 'mutter-clutter-' + mutter_api_version
cogl_pc = 'mutter-cogl-' + mutter_api_version
cogl_pango_pc = 'mutter-cogl-pango-' + mutter_api_version
libmutter_pc = 'libmutter-' + mutter_api_version

croco_req = '>= 0.6.8'
ecal_req = '>= 3.5.3'
eds_req = '>= 3.17.2'
gcr_req = '>= 3.7.5'
gdesktop_req = '>= 3.7.90'
gio_req = '>= 2.56.0'
gi_req = '>= 1.49.1'
gjs_req = '>= 1.47.0'
gtk_req = '>= 3.15.0'
json_glib_req = '>= 0.13.2'
mutter_req = '>= 3.30.1'
polkit_req = '>= 0.100'
schemas_req = '>= 3.27.90'
startup_req = '>= 0.11'
ibus_req = '>= 1.5.2'

bt_req = '>= 3.9.0'
gst_req = '>= 0.11.92'
nm_req = '>= 1.10.4'
secret_req = '>= 0.18'

gnome = import('gnome')
i18n  = import('i18n')

prefix = get_option('prefix')

bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
mandir = join_paths(prefix, get_option('mandir'))
sysconfdir = join_paths(prefix, get_option('sysconfdir'))

pkgdatadir = join_paths(datadir, meson.project_name())
pkglibdir = join_paths(libdir, meson.project_name())

autostartdir = join_paths(sysconfdir, 'xdg', 'autostart')
convertdir = join_paths(datadir, 'GConf', 'gsettings')
desktopdir = join_paths(datadir, 'applications')
ifacedir = join_paths(datadir, 'dbus-1', 'interfaces')
localedir = join_paths(datadir, 'locale')
mozplugindir = join_paths(libdir, 'mozilla', 'plugins')
portaldir = join_paths(datadir, 'xdg-desktop-portal', 'portals')
schemadir = join_paths(datadir, 'glib-2.0', 'schemas')
servicedir = join_paths(datadir, 'dbus-1', 'services')

plugindir = get_variable('BROWSER_PLUGIN_DIR', mozplugindir)

# XXX: Once https://github.com/systemd/systemd/issues/9595 is fixed and we can
# depend on this version, replace with something like:
# systemduserunitdir = systemd_dep.get_pkgconfig_variable('systemduserunitdir',
#                                                         define_variable: ['prefix', prefix])
# and uncomment systemd_dep below
systemduserunitdir = join_paths(prefix, 'lib', 'systemd', 'user')

keybindings_dep = dependency('gnome-keybindings', required: false)
if keybindings_dep.found()
  keysdir = keybindings_dep.get_pkgconfig_variable('keysdir')
else
  keysdir = join_paths(datadir, 'gnome-control-center', 'keybindings')
endif

atk_bridge_dep = dependency('atk-bridge-2.0')
canberra_dep = dependency('libcanberra')
canberra_gtk_dep = dependency('libcanberra-gtk3')
ecal_dep = dependency('libecal-1.2', version: ecal_req)
eds_dep = dependency('libedataserver-1.2', version: eds_req)
gcr_dep = dependency('gcr-base-3', version: gcr_req)
gdk_x11_dep = dependency('gdk-x11-3.0')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0')
gi_dep = dependency('gobject-introspection-1.0', version: gi_req)
gio_dep = dependency('gio-2.0', version: gio_req)
gio_unix_dep = dependency('gio-unix-2.0', version: gio_req)
gjs_dep = dependency('gjs-1.0', version: gjs_req)
gtk_dep = dependency('gtk+-3.0', version: gtk_req)
libxml_dep = dependency('libxml-2.0')
croco_dep = dependency('libcroco-0.6', version: croco_req)
clutter_dep = dependency(clutter_pc, version: mutter_req)
cogl_dep = dependency(cogl_pc, version: mutter_req)
cogl_pango_dep = dependency(cogl_pango_pc, version: mutter_req)
mutter_dep = dependency(libmutter_pc, version: mutter_req)
polkit_dep = dependency('polkit-agent-1', version: polkit_req)
startup_dep = dependency('libstartup-notification-1.0', version: startup_req)
ibus_dep = dependency('ibus-1.0', version: ibus_req)
x11_dep = dependency('x11')
schemas_dep = dependency('gsettings-desktop-schemas', version: schemas_req)

bt_dep = dependency('gnome-bluetooth-1.0', version: bt_req, required: false)
gst_dep = dependency('gstreamer-1.0', version: gst_req, required: false)
gst_base_dep = dependency('gstreamer-base-1.0', required: false)

recorder_deps = []
enable_recorder = gst_dep.found() and gst_base_dep.found()
if enable_recorder
  recorder_deps += [gst_dep, gst_base_dep, gtk_dep, x11_dep]
endif

nm_deps = []
if get_option('networkmanager')
  nm_deps += dependency('libnm', version: nm_req)
  nm_deps += dependency('libsecret-1', version: secret_req)

  vpndir = nm_deps[0].get_pkgconfig_variable('vpnservicedir')

  have_networkmanager = true
else
  vpndir = prefix

  have_networkmanager = false
endif

if get_option('systemd')
  libsystemd_dep = dependency('libsystemd')
  # XXX: see systemduserunitdir
  # systemd_dep = dependency('systemd')
  have_systemd = true
else
  libsystemd_dep = []
  have_systemd = false
endif

if get_option('browser_plugin')
  json_glib_dep = dependency('json-glib-1.0', version: json_glib_req)

  subdir('browser-plugin')
endif

if get_option('man')
  xsltproc = find_program('xsltproc')

  subdir('man')
endif

mutter_typelibdir = mutter_dep.get_pkgconfig_variable('typelibdir')
python = find_program('python3')
sassc = find_program('sassc')

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required: false)

cdata = configuration_data()
cdata.set_quoted('GETTEXT_PACKAGE', meson.project_name())
cdata.set_quoted('VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())

cdata.set('HAVE_NETWORKMANAGER', have_networkmanager)
cdata.set('HAVE_SYSTEMD', have_systemd)

# New API added in glib-2.57.2
cdata.set('HAVE_GIO_DESKTOP_LAUNCH_URIS_WITH_FDS',
          cc.has_function('g_desktop_app_info_launch_uris_as_manager_with_fds',
                          dependencies : gio_dep)
)
cdata.set('HAVE_FDWALK', cc.has_function('fdwalk'))
cdata.set('HAVE_MALLINFO', cc.has_function('mallinfo'))
cdata.set('HAVE_SYS_RESOURCE_H', cc.has_header('sys/resource.h'))
cdata.set('HAVE__NL_TIME_FIRST_WEEKDAY',
  cc.has_header_symbol('langinfo.h', '_NL_TIME_FIRST_WEEKDAY')
)

cdata.set('HAVE_FDWALK',
          cc.has_function('fdwalk')
)

config_h = configure_file(
  input: 'config.h.meson',
  output: 'config.h',
  configuration: cdata
)

conf_inc = include_directories('.')

libgvc = subproject('gvc',
  default_options: [
    'package_name=' + meson.project_name(),
    'package_version=' + meson.project_version(),
    'pkgdatadir=' + pkgdatadir,
    'pkglibdir=' + pkglibdir,
    'static=false',
    'introspection=true',
    'alsa=false'
  ]
)
libgvc_gir = libgvc.get_variable('libgvc_gir')

subdir('js')
subdir('src')
subdir('po')
subdir('data')
subdir('tests')
subdir('tools')

if get_option('gtk_doc')
  subdir('docs/reference')
endif

meson.add_install_script('meson/meson-postinstall.sh')
