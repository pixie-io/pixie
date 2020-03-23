apt_update 'update'

apt_pkg_list = [
  'autoconf',
  'bash-completion',
  'build-essential',
  'checkstyle',
  'curl',
  # Not the newest docker CE from official docker repository, but should suffice.
  'docker.io',
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
  'systemd',
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

execute 'enable docker' do
  command 'systemctl enable docker'
  action :run
end

apt_repository 'gcc-9.2-ppa' do
   uri         'ppa:jonathonf/gcc-9.2'
end

apt_update 'update packages' do
  action :update
end

apt_package ['gcc-9','g++-9'] do
  action :upgrade
end

include_recipe 'pixielabs::linux_java'
include_recipe 'pixielabs::linux_clang'
