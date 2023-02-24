# please, keep this sorted alphabetically
st_headers = [
  'st-adjustment.h',
  'st-bin.h',
  'st-border-image.h',
  'st-box-layout.h',
  'st-button.h',
  'st-clipboard.h',
  'st-drawing-area.h',
  'st-entry.h',
  'st-focus-manager.h',
  'st-generic-accessible.h',
  'st-icon.h',
  'st-icon-colors.h',
  'st-icon-theme.h',
  'st-image-content.h',
  'st-label.h',
  'st-password-entry.h',
  'st-scrollable.h',
  'st-scroll-bar.h',
  'st-scroll-view.h',
  'st-scroll-view-fade.h',
  'st-settings.h',
  'st-shadow.h',
  'st-texture-cache.h',
  'st-theme.h',
  'st-theme-context.h',
  'st-theme-node.h',
  'st-types.h',
  'st-viewport.h',
  'st-widget.h',
  'st-widget-accessible.h'
]

st_includes = []
foreach include : st_headers
  st_includes += '#include <@0@>'.format(include)
endforeach

st_h_data = configuration_data()
st_h_data.set('includes', '\n'.join(st_includes))

st_h = configure_file(
  input: 'st.h.in',
  output: 'st.h',
  configuration: st_h_data
)

st_inc = include_directories('.', '..')

# please, keep this sorted alphabetically
st_private_headers = [
  'croco/cr-additional-sel.h',
  'croco/cr-attr-sel.h',
  'croco/cr-cascade.h',
  'croco/cr-declaration.h',
  'croco/cr-doc-handler.h',
  'croco/cr-enc-handler.h',
  'croco/cr-fonts.h',
  'croco/cr-input.h',
  'croco/cr-num.h',
  'croco/cr-om-parser.h',
  'croco/cr-parser.h',
  'croco/cr-parsing-location.h',
  'croco/cr-prop-list.h',
  'croco/cr-pseudo.h',
  'croco/cr-rgb.h',
  'croco/cr-selector.h',
  'croco/cr-simple-sel.h',
  'croco/cr-statement.h',
  'croco/cr-string.h',
  'croco/cr-stylesheet.h',
  'croco/cr-term.h',
  'croco/cr-tknzr.h',
  'croco/cr-token.h',
  'croco/cr-utils.h',
  'croco/libcroco-config.h',
  'croco/libcroco.h',
  'st-private.h',
  'st-theme-private.h',
  'st-theme-node-private.h',
  'st-theme-node-transition.h'
]

# please, keep this sorted alphabetically
croco_sources = [
  'croco/cr-additional-sel.c',
  'croco/cr-attr-sel.c',
  'croco/cr-cascade.c',
  'croco/cr-declaration.c',
  'croco/cr-doc-handler.c',
  'croco/cr-enc-handler.c',
  'croco/cr-fonts.c',
  'croco/cr-input.c',
  'croco/cr-num.c',
  'croco/cr-om-parser.c',
  'croco/cr-parser.c',
  'croco/cr-parsing-location.c',
  'croco/cr-prop-list.c',
  'croco/cr-pseudo.c',
  'croco/cr-rgb.c',
  'croco/cr-selector.c',
  'croco/cr-simple-sel.c',
  'croco/cr-statement.c',
  'croco/cr-string.c',
  'croco/cr-stylesheet.c',
  'croco/cr-term.c',
  'croco/cr-tknzr.c',
  'croco/cr-token.c',
  'croco/cr-utils.c',
]

# please, keep this sorted alphabetically
st_sources = [
  'st-adjustment.c',
  'st-bin.c',
  'st-border-image.c',
  'st-box-layout.c',
  'st-button.c',
  'st-clipboard.c',
  'st-drawing-area.c',
  'st-entry.c',
  'st-focus-manager.c',
  'st-generic-accessible.c',
  'st-icon.c',
  'st-icon-cache.c',
  'st-icon-colors.c',
  'st-icon-theme.c',
  'st-image-content.c',
  'st-label.c',
  'st-password-entry.c',
  'st-private.c',
  'st-scrollable.c',
  'st-scroll-bar.c',
  'st-scroll-view.c',
  'st-scroll-view-fade.c',
  'st-settings.c',
  'st-shadow.c',
  'st-texture-cache.c',
  'st-theme.c',
  'st-theme-context.c',
  'st-theme-node.c',
  'st-theme-node-drawing.c',
  'st-theme-node-transition.c',
  'st-viewport.c',
  'st-widget.c'
]

st_enums = gnome.mkenums_simple('st-enum-types',
  sources: st_headers,
  header_prefix: '''
#if !defined(ST_H_INSIDE) && !defined(ST_COMPILATION)
#error "Only <st/st.h> can be included directly.h"
#endif'''
)

st_gir_sources = st_sources + st_headers + st_enums

data_to_c = find_program(meson.project_source_root() + '/src/data-to-c.pl')

glsl_sources = custom_target('scroll-view-fade-glsl',
  input: ['st-scroll-view-fade.glsl'],
  output: ['st-scroll-view-fade-generated.h'],
  capture: true,
  command: [data_to_c, '@INPUT@', 'st_scroll_view_fade_glsl']
)

st_nogir_sources = [glsl_sources]

st_cflags = [
  '-I@0@/src'.format(meson.project_source_root()),
  '-I@0@'.format(meson.project_build_root()),
  '-DPREFIX="@0@"'.format(prefix),
  '-DLIBDIR="@0@"'.format(libdir),
  '-DG_LOG_DOMAIN="St"',
  '-DST_COMPILATION',
  '-DCLUTTER_ENABLE_EXPERIMENTAL_API',
  '-DCOGL_ENABLE_EXPERIMENTAL_API',
  '-DPACKAGE_DATA_DIR="@0@"'.format(pkgdatadir)
]

# Currently meson requires a shared library for building girs
libst = shared_library('st-' + mutter_api_version,
  sources: st_gir_sources + st_nogir_sources + croco_sources,
  c_args: st_cflags,
  dependencies: [clutter_dep, mutter_dep, libxml_dep, gdk_pixbuf_dep, m_dep],
  build_rpath: mutter_typelibdir,
  install_rpath: mutter_typelibdir,
  install_dir: pkglibdir,
  install: true
)

libst_dep = declare_dependency(link_with: libst,
  sources: st_enums[1]
)

if get_option('tests')
  mutter_test_dep = dependency(libmutter_test_pc, version: mutter_req)
  test_theme = executable('test-theme',
    sources: 'test-theme.c',
    c_args: st_cflags,
    dependencies: [mutter_test_dep, gtk_dep, libxml_dep],
    build_rpath: mutter_typelibdir,
    link_with: libst
  )

  test('CSS styling support', test_theme,
    workdir: meson.current_source_dir(),
  )
endif

libst_gir = gnome.generate_gir(libst,
  sources: st_gir_sources,
  nsversion: mutter_api_version,
  namespace: 'St',
  includes: [
    'Clutter-' + mutter_api_version,
    'Cally-' + mutter_api_version,
    'GdkPixbuf-2.0',
    'Meta-' + mutter_api_version],
  dependencies: [mutter_dep, gdk_pixbuf_dep],
  include_directories: include_directories('..'),
  extra_args: ['-DST_COMPILATION', '--quiet'],
  install_dir_gir: pkgdatadir,
  install_dir_typelib: pkglibdir,
  install: true
)
