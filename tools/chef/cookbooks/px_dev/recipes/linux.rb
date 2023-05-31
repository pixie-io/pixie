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

if ! platform_family?('debian')
  return
end

apt_update 'update'

apt_pkg_list = [
  'autoconf',
  'bash-completion',
  'bc',
  'build-essential',
  'crun',
  'curl',
  # Not the newest docker CE from official docker repository, but should suffice.
  'docker.io',
  'git',
  'libncurses5',
  'lcov',
  'podman',
  'sudo',
  'systemd',
  'unzip',
  'virtualenvwrapper',
  'zlib1g-dev',
  'zip',

  'bison',
  'flex',
  'libedit-dev',
  'libelf-dev',

  'gcc-12',
  'g++-12',

  # Libtool/unwind, needed for perftools.
  'libltdl-dev',
  'libunwind-dev',

  'qemu',
  'qemu-system-arm',
  'qemu-system-x86',
  'qemu-user-static',
  'qemu-utils',
]

apt_package apt_pkg_list do
  action :upgrade
end

execute 'enable docker' do
  command 'systemctl enable docker'
  action :run
end
