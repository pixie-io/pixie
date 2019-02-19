apt_update 'update'

apt_pkg_list = [
  'bash-completion',
  'build-essential',
  'curl',
  'git',
  'golang',
  'lcov',
  'php',
  'php-curl',
  'python',
  'python3-pip',
  'python3.6',
  'python3.6-dev',
  'unzip',
  'virtualenvwrapper',
  'zlib1g-dev',

  'bison',
  'cmake',
  'flex',
  'libedit-dev',
  'libelf-dev'
]

apt_package apt_pkg_list do
  action :upgrade
end

include_recipe 'pixielabs::linux_java'
include_recipe 'pixielabs::linux_bazel'
include_recipe 'pixielabs::linux_bcc'
include_recipe 'pixielabs::linux_clang'
