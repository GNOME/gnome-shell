project('shew', 'c',
  version: '3.36.9',
  meson_version: '>= 0.47.0',
  license: 'LGPLv2+',
)

assert(meson.is_subproject(), 'This project is only intended to be used as a subproject!')

gnome = import('gnome')
pkg = import('pkgconfig')

api_version = '0'
full_name = '@0@-@1@'.format(meson.project_name(), api_version)

package_version = meson.project_version()
package_name = get_option('package_name')
assert(package_name != '', 'package_name must be specified')

pkgdatadir = join_paths(get_option('datadir'), package_name)
pkglibdir = join_paths(get_option('libdir'), package_name)

girdir = join_paths(pkgdatadir, 'gir-1.0')
typelibdir = join_paths(pkglibdir, 'girepository-1.0')

gtk_dep = dependency('gtk+-3.0')

subdir('src')
