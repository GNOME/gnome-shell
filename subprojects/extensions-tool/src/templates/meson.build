template_metas = [
  '00-plain.desktop',
  'indicator.desktop',
]
template_deps = []
foreach template : template_metas
  template_deps += i18n.merge_file(
    input: template + '.in',
    output: template,
    po_dir: po_dir,
    type: 'desktop',
  )
endforeach
