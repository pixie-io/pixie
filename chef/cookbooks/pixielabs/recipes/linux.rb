apt_update 'update'

apt_pkg_list = [
  'bash-completion',
  'build-essential',
  'curl',
  'doxygen',
  'git',
  'lcov',
  'php',
  'php-curl',
  'python',
  'python-pip',
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
  'libelf-dev',

  # Libtool/unwind, needed for perftools.
  'libltdl-dev',
  'libunwind-dev',
]

apt_package apt_pkg_list do
  action :upgrade
end

include_recipe 'pixielabs::linux_java'
include_recipe 'pixielabs::linux_bazel'
include_recipe 'pixielabs::linux_bcc'
include_recipe 'pixielabs::linux_clang'
