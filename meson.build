project('gnome-shell', 'c',
  version: '3.36.9',
  meson_version: '>= 0.47.0',
  license: 'GPLv2+'
)

split_version = meson.project_version().split('.')

# We depend on a specific version of the libmutter API. The mutter variants of
# the Cogl and Clutter libraries also use this API version.
# The API version is increased automatically each development cycle,
# starting with 0 in 3.23.x
api_version = (split_version[1].to_int() - 23) / 2
mutter_api_version = '@0@'.format(api_version)

clutter_pc = 'mutter-clutter-' + mutter_api_version
cogl_pc = 'mutter-cogl-' + mutter_api_version
cogl_pango_pc = 'mutter-cogl-pango-' + mutter_api_version
libmutter_pc = 'libmutter-' + mutter_api_version

ecal_req = '>= 3.33.1'
eds_req = '>= 3.33.1'
gcr_req = '>= 3.7.5'
gio_req = '>= 2.56.0'
gi_req = '>= 1.49.1'
gjs_req = '>= 1.63.2'
gtk_req = '>= 3.15.0'
mutter_req = '>= 3.36.0'
polkit_req = '>= 0.100'
schemas_req = '>= 3.33.1'
startup_req = '>= 0.11'
ibus_req = '>= 1.5.2'
gnome_desktop_req = '>= 3.35.90'

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
icondir = join_paths(datadir, 'icons')
ifacedir = join_paths(datadir, 'dbus-1', 'interfaces')
localedir = join_paths(datadir, 'locale')
metainfodir = join_paths(datadir, 'metainfo')
portaldir = join_paths(datadir, 'xdg-desktop-portal', 'portals')
schemadir = join_paths(datadir, 'glib-2.0', 'schemas')
servicedir = join_paths(datadir, 'dbus-1', 'services')

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
ecal_dep = dependency('libecal-2.0', version: ecal_req)
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
clutter_dep = dependency(clutter_pc, version: mutter_req)
cogl_dep = dependency(cogl_pc, version: mutter_req)
cogl_pango_dep = dependency(cogl_pango_pc, version: mutter_req)
mutter_dep = dependency(libmutter_pc, version: mutter_req)
polkit_dep = dependency('polkit-agent-1', version: polkit_req)
startup_dep = dependency('libstartup-notification-1.0', version: startup_req)
ibus_dep = dependency('ibus-1.0', version: ibus_req)
x11_dep = dependency('x11')
schemas_dep = dependency('gsettings-desktop-schemas', version: schemas_req)
gnome_desktop_dep = dependency('gnome-desktop-3.0', version: gnome_desktop_req)

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

  have_networkmanager = true
else
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

if get_option('man')
  xsltproc = find_program('xsltproc')

  subdir('man')
endif

mutter_typelibdir = mutter_dep.get_pkgconfig_variable('typelibdir')
python = find_program('python3')
sassc = find_program('sassc')
gjs = find_program('gjs')

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

buildtype = get_option('buildtype')
if buildtype != 'plain'
  all_warnings = [
    '-fno-strict-aliasing',
    '-Wpointer-arith',
    '-Wmissing-declarations',
    '-Wimplicit-function-declaration',
    '-Wformat=2',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wstrict-prototypes',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wundef',
    '-Wunused',
    '-Wcast-align',
    '-Wmissing-noreturn',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wlogical-op',
    '-Wignored-qualifiers',
    '-Werror=redundant-decls',
    '-Werror=implicit',
    '-Werror=nonnull',
    '-Werror=init-self',
    '-Werror=main',
    '-Werror=missing-braces',
    '-Werror=sequence-point',
    '-Werror=return-type',
    '-Werror=trigraphs',
    '-Werror=array-bounds',
    '-Werror=write-strings',
    '-Werror=address',
    '-Werror=int-to-pointer-cast',
    '-Werror=pointer-to-int-cast',
    '-Werror=empty-body',
    '-Werror=write-strings',
  ]
  supported_warnings = cc.get_supported_arguments(all_warnings)
  add_project_arguments(supported_warnings, language: 'c')
endif

if get_option('debug')
  debug_c_args = [
    '-DG_ENABLE_DEBUG',
    '-fno-omit-frame-pointer'
  ]
  supported_debug_c_args = cc.get_supported_arguments(debug_c_args)
  add_project_arguments(supported_debug_c_args, language: 'c')
endif

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

libshew = subproject('shew',
  default_options: [
    'package_name=@0@'.format(meson.project_name()),
  ]
)

libshew_version = libshew.get_variable('package_version')
assert(libshew_version == meson.project_version(),
  'shew version does not match project version')

if get_option('extensions_tool')
  extension_tool = subproject('extensions-tool',
    default_options: [
      'package_name=@0@'.format(meson.project_name()),
    ]
  )

  extension_tool_version = extension_tool.get_variable('package_version')
  assert(extension_tool_version == meson.project_version(),
    'extension-tool version does not match project version'
  )
endif

if get_option('extensions_app')
  extensions_app = subproject('extensions-app',
    default_options: [
      'package_name=@0@'.format(meson.project_name()),
    ]
  )

  extensions_app_version = extensions_app.get_variable('package_version')
  assert(extensions_app_version == meson.project_version(),
    'Extensions app version does not match project version'
  )
endif


po_dir = join_paths(meson.current_source_dir(), 'po')

subdir('js')
subdir('src')
subdir('po')
subdir('data')
subdir('tests')
subdir('tools')

if get_option('gtk_doc')
  subdir('docs/reference')
endif

meson.add_install_script('meson/postinstall.py')

summary_options = {
  'networkmanager': get_option('networkmanager'),
  'systemd': get_option('systemd'),
  'extensions_app': get_option('extensions_app'),
  'extensions_tool': get_option('extensions_tool'),
  'man': get_option('man'),
  'gtk_doc': get_option('gtk_doc'),
}

summary_build = {
  'buildtype': get_option('buildtype'),
  'debug': get_option('debug'),
}

summary_dirs = {
  'prefix': get_option('prefix'),
  'bindir': get_option('bindir'),
  'libdir': get_option('libdir'),
  'libexecdir': get_option('libexecdir'),
  'datadir': get_option('datadir'),
  'sysconfdir': get_option('sysconfdir'),
}

if get_option('man')
  summary_dirs += { 'mandir': get_option('mandir') }
endif

if meson.version().version_compare('>= 0.53.0')
  summary(summary_dirs, section: 'Directories')
  summary(summary_build, section: 'Build Configuration')
  summary(summary_options, section: 'Build Options')
endif
