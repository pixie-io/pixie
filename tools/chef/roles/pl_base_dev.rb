name 'pl_base_dev'
description 'This role installs all relevant developer desktop apps'

run_list(
  'recipe[pixielabs::base]',
)
