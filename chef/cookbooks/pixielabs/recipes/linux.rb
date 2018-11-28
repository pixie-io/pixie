apt_update 'update'

apt_pkg_list = [
  'bash-completion',
  'build-essential',
  'clang',
  'clang-format',
  'curl',
  'git',
  'golang',
  'php',
  'php-curl',
  'python',
  'python3-pip',
  'python3.6',
  'python3.6-dev',
  'unzip',
  'virtualenvwrapper',
  'zlib1g-dev',
]

apt_package apt_pkg_list do
  action :upgrade
end

include_recipe 'pixielabs::linux_java'
include_recipe 'pixielabs::linux_bazel'
