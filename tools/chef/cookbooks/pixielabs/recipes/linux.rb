# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

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
  'python3-pip',
  'python3.8',
  'python3.8-dev',
  'systemd',
  'unzip',
  'virtualenvwrapper',
  'zlib1g-dev',
  'zip',

  'bison',
  'cmake',
  'flex',
  'libedit-dev',
  'libelf-dev',

  # Libtool/unwind, needed for perftools.
  'libltdl-dev',
  'libunwind-dev',

  # Needed by Clang-10.
  'libz3-4',
  'libz3-dev',
]

apt_package apt_pkg_list do
  action :upgrade
end

execute 'enable docker' do
  command 'systemctl enable docker'
  action :run
end

apt_repository 'ubuntu-toolchain-ppa' do
   uri         'ppa:ubuntu-toolchain-r/ppa'
end

apt_update 'update packages' do
  action :update
end

apt_package ['gcc-10','g++-10'] do
  action :upgrade
end

execute 'python alternatives selection' do
  command 'update-alternatives --install /usr/bin/python python /usr/bin/python3 100'
end

include_recipe 'pixielabs::linux_java'
include_recipe 'pixielabs::linux_clang'
