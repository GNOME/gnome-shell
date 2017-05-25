theme_sources = files([
  'gnome-shell-high-contrast.scss',
  'gnome-shell.scss',
  'gnome-shell-sass/_colors.scss',
  'gnome-shell-sass/_common.scss',
  'gnome-shell-sass/_drawing.scss',
  'gnome-shell-sass/_high-contrast-colors.scss'
])

theme_deps = []

if sassc.found()
  parse_sass = files('parse-sass.sh')

  theme_deps += custom_target('update-theme',
    output: 'theme-update.stamp',
    depend_files: theme_sources,
    command: [parse_sass, '@OUTPUT@'],
    build_by_default: true
  )
endif
