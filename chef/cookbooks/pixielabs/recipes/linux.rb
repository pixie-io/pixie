apt_update 'update'

apt_pkg_list = [
  'autoconf',
  'bash-completion',
  'build-essential',
  'checkstyle',
  'curl',
  'doxygen',
  'git',
  'graphviz',
  'lcov',
  'php',
  'php-curl',
  'php-xml',
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
include_recipe 'pixielabs::linux_clang'
