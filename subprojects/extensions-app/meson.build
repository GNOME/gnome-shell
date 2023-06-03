project('gnome-extensions-app',
  version: '44.2',
  meson_version: '>= 0.58.0',
  license: 'GPLv2+'
)

if get_option('profile') == 'development'
  profile = '.Devel'
  vcs_tag = run_command('git', 'rev-parse', '--short', '@',
    check: false,
  ).stdout().strip()
else
  profile = ''
  vcs_tag = ''
endif

base_id = 'org.gnome.Extensions'
app_id = base_id + profile
prgname = 'gnome-extensions-app'

gnome = import('gnome')
i18n  = import('i18n')

if meson.is_subproject()
  package_name = get_option('package_name')
  assert(package_name != '',
    'package_name must be specified for subproject builds')

  po_dir = join_paths(meson.current_source_dir(), '../../po')
else
  package_name = meson.project_name()
  po_dir = join_paths(meson.current_source_dir(), 'po')
endif

package_version = meson.project_version()
prefix = get_option('prefix')

bindir = join_paths(prefix, get_option('bindir'))
libdir = join_paths(prefix, get_option('libdir'))
datadir = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, package_name)

desktopdir = join_paths(datadir, 'applications')
icondir = join_paths(datadir, 'icons')
localedir = join_paths(datadir, 'locale')
metainfodir = join_paths(datadir, 'metainfo')
servicedir = join_paths(datadir, 'dbus-1', 'services')

gjs = find_program('gjs')
appstream_util = find_program('appstream-util', required: false)
desktop_file_validate = find_program('desktop-file-validate', required: false)

subdir('data')
subdir('js')

if not meson.is_subproject()
  subproject('shew',
    default_options: [
      'package_name=@0@'.format(meson.project_name()),
    ]
  )

  subdir('po')

  gnome.post_install(
    gtk_update_icon_cache: true
  )

  if appstream_util.found()
    meson.add_dist_script('build-aux/meson/check-version.py',
      meson.project_version(),
      '--type=metainfo',
      'data/metainfo/org.gnome.Extensions.metainfo.xml.in')
  endif

  summary_dirs = {
    'prefix': get_option('prefix'),
    'bindir': get_option('bindir'),
    'libdir': get_option('bindir'),
    'datadir': get_option('datadir'),
  }

  summary_build = {
    'buildtype': get_option('buildtype'),
    'debug': get_option('debug'),
  }

  summary(summary_dirs, section: 'Directories')
  summary(summary_build, section: 'Build Configuration')
endif
