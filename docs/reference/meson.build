gidocgen = find_program('gi-docgen', required: get_option('gtk_doc'))

doc_conf = configuration_data()
doc_conf.set('MUTTER_API_VERSION', mutter_api_version)

gi_docgen_common_args = [
  '--quiet',
  '--no-namespace-dir',
]

if get_option('werror')
  gi_docgen_common_args += ['--fatal-warnings']
endif

docs_dir = datadir / 'doc'

subdir('shell')
subdir('st')
