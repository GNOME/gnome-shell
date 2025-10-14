project('gnome-shell', 'c',
  version: '49.1',
  meson_version: '>= 1.3.0',
  license: 'GPL-2.0-or-later',
)

split_version = meson.project_version().split('.')

# We depend on a specific version of the libmutter API. The mutter variants of
# the Cogl and Clutter libraries also use this API version.
# The API version is increased each development cycle,
# starting with 0 in 3.23.x
mutter_api_version = '18'

clutter_pc = 'mutter-clutter-' + mutter_api_version
cogl_pc = 'mutter-cogl-' + mutter_api_version
mtk_pc = 'mutter-mtk-' + mutter_api_version
libmutter_pc = 'libmutter-' + mutter_api_version
libmutter_test_pc = 'libmutter-test-' + mutter_api_version

ecal_req = '>= 3.33.1'
eds_req = '>= 3.33.1'
gcr_req = '>= 3.90.0'
gio_req = '>= 2.86.0'
gjs_req = '>= 1.85.90'
gtk_req = '>= 4.0'
mutter_req = '>= 49.0'
polkit_req = '>= 0.100'
schemas_req = '>= 49.alpha'
systemd_req = '>= 246'
gnome_desktop_req = '>= 40'
pipewire_req = '>= 0.3.49'
pango_req = '>= 1.46.0'

nm_req = '>= 1.10.4'
secret_req = '>= 0.18'

fs = import('fs')
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
desktopdir = join_paths(datadir, 'applications')
icondir = join_paths(datadir, 'icons')
ifacedir = join_paths(datadir, 'dbus-1', 'interfaces')
localedir = join_paths(datadir, 'locale')
metainfodir = join_paths(datadir, 'metainfo')
schemadir = join_paths(datadir, 'glib-2.0', 'schemas')
servicedir = join_paths(datadir, 'dbus-1', 'services')

keybindings_dep = dependency('gnome-keybindings', required: false)
if keybindings_dep.found()
  keysdir = keybindings_dep.get_variable('keysdir', pkgconfig_define: ['datadir', datadir])
else
  keysdir = join_paths(datadir, 'gnome-control-center', 'keybindings')
endif

atk_bridge_dep = dependency('atk-bridge-2.0')
ecal_dep = dependency('libecal-2.0', version: ecal_req)
eds_dep = dependency('libedataserver-1.2', version: eds_req)
gcr_dep = dependency('gcr-4', version: gcr_req)
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0')
gi_dep = dependency('girepository-2.0', version: gio_req)
gio_dep = dependency('gio-2.0', version: gio_req)
gio_unix_dep = dependency('gio-unix-2.0', version: gio_req)
gjs_dep = dependency('gjs-1.0', version: gjs_req)
gtk_dep = dependency('gtk4', version: gtk_req)
libxml_dep = dependency('libxml-2.0')
clutter_dep = dependency(clutter_pc, version: mutter_req)
mtk_dep = dependency(mtk_pc, version: mutter_req)
cogl_dep = dependency(cogl_pc, version: mutter_req)
mutter_dep = dependency(libmutter_pc, version: mutter_req)
polkit_dep = dependency('polkit-agent-1', version: polkit_req)
schemas_dep = dependency('gsettings-desktop-schemas', version: schemas_req)
gnome_desktop_dep = dependency('gnome-desktop-4', version: gnome_desktop_req)
pango_dep = dependency('pango', version: pango_req)

have_fonts = mutter_dep.get_variable('have_fonts') == 'true'
have_x11 = mutter_dep.get_variable('have_x11') == 'true'
have_x11_client = mutter_dep.get_variable('have_x11_client') == 'true'
if have_x11_client or have_x11
  x11_dep = dependency('x11')
  xext_dep = dependency('xext')
  xfixes_dep = dependency('xfixes')
endif

if not have_fonts
  error('Shell requires mutter to be built with fonts rendering support')
endif


nm_deps = []
if get_option('networkmanager')
  nm_deps += dependency('libnm', version: nm_req)
  nm_deps += dependency('libsecret-1', version: secret_req)

  have_networkmanager = true
else
  have_networkmanager = false
endif

have_portal_helper = get_option('portal_helper')
if have_portal_helper and not have_networkmanager
  error('Portal helper requires networkmanager support')
endif

if get_option('camera_monitor')
  libpipewire_dep = dependency('libpipewire-0.3', version: pipewire_req)
  have_pipewire = true
else
  libpipewire_dep = []
  have_pipewire = false
endif

if get_option('systemd')
  libsystemd_dep = dependency('libsystemd', version: systemd_req)
  systemd_dep = dependency('systemd', version: systemd_req)
  systemduserunitdir = systemd_dep.get_variable('systemduserunitdir',
    pkgconfig_define: ['prefix', prefix])
  have_systemd = true
else
  libsystemd_dep = []
  have_systemd = false
endif

if get_option('man')
  rst2man = find_program('rst2man')
  subdir('man')
endif

mutter_typelibdir = mutter_dep.get_variable('typelibdir')
python = find_program('python3')
gjs = find_program('gjs')
generate_app_list = find_program('build-aux/generate-app-list.py')

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required: false)

cdata = configuration_data()
cdata.set_quoted('GETTEXT_PACKAGE', meson.project_name())
cdata.set_quoted('VERSION', meson.project_version())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())

cdata.set('HAVE_NETWORKMANAGER', have_networkmanager)
cdata.set('HAVE_PIPEWIRE', have_pipewire)
cdata.set('HAVE_SYSTEMD', have_systemd)
cdata.set('HAVE_X11', have_x11)
cdata.set('HAVE_X11_CLIENT', have_x11_client)

cdata.set('HAVE_FDWALK', cc.has_function('fdwalk'))
cdata.set('HAVE_MALLINFO', cc.has_function('mallinfo'))
cdata.set('HAVE_MALLINFO2', cc.has_function('mallinfo2'))
cdata.set('HAVE_SYS_RESOURCE_H', cc.has_header('sys/resource.h'))
cdata.set('HAVE_EXE_INTROSPECTION',
  cc.has_header('elf.h') and
  cc.has_header('link.h') and
  cc.has_define('__GLIBC__', prefix: '#include <link.h>')
)
cdata.set('HAVE__NL_TIME_FIRST_WEEKDAY',
  cc.has_header_symbol('langinfo.h', '_NL_TIME_FIRST_WEEKDAY')
)

cdata.set('HAVE_FDWALK',
          cc.has_function('fdwalk')
)

polkit_has_autocleanup = cc.compiles(
'''
  #define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
  #include <polkitagent/polkitagent.h>
  void main(void) { g_autoptr(PolkitAgentListener) agent = NULL; }
''',
  dependencies: polkit_dep)
cdata.set('HAVE_POLKIT_AUTOCLEANUP', polkit_has_autocleanup)

buildtype = get_option('buildtype')
if buildtype != 'plain'
  gs_c_args = [
    '-fno-omit-frame-pointer',
    '-mno-omit-leaf-frame-pointer',
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


  if get_option('debug')
    gs_c_args += [
      '-DG_ENABLE_DEBUG',
    ]
  else
    gs_c_args += [
      '-DG_DISABLE_ASSERT',
      '-DG_DISABLE_CAST_CHECKS',
    ]
  endif

  supported_gs_c_args = cc.get_supported_arguments(gs_c_args)
  add_project_arguments(supported_gs_c_args, language: 'c')
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

libshew = dependency('libshew',
  default_options: [
    'package_name=@0@'.format(meson.project_name()),
  ]
)

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

if get_option('tests')
  common_test_env = environment()
  common_test_env.set('GSETTINGS_SCHEMA_DIR', meson.project_build_root() / 'data')
  common_test_env.set('GSETTINGS_BACKEND', 'memory')
  common_test_env.set('XDG_CURRENT_DESKTOP', '')

  jasmine = find_program('jasmine')

  add_test_setup('default',
    is_default: true,
    env: common_test_env,
  )
endif

po_dir = join_paths(meson.current_source_dir(), 'po')

subdir('data')
subdir('js')
subdir('src')
subdir('po')

if get_option('tests') and have_x11_client
  subdir('tests')
endif

if get_option('gtk_doc')
  subdir('docs/reference')
endif

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true
)

meson.add_dist_script('build-aux/generate-stylesheets.py')
meson.add_dist_script('build-aux/check-version.py', meson.project_version(), 'NEWS')

summary_options = {
  'x11': have_x11,
  'x11_client': have_x11_client,
  'camera_monitor': get_option('camera_monitor'),
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

summary(summary_dirs, section: 'Directories')
summary(summary_build, section: 'Build Configuration')
summary(summary_options, section: 'Build Options')
