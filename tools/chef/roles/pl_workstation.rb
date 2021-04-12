name 'pl_base_dev'
description 'The base workstation setup (without GUI apps)'

run_list(
  'recipe[pixielabs]',
  'recipe[pixielabs::dev_extras]'
)
