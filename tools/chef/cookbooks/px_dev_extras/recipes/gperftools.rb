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


if platform_family?('mac_os_x')
  homebrew_package 'gperftools'
  return
end

if ! platform_family?('debian')
  return
end

remote_file '/tmp/gperftools.deb' do
  source node['gperftools']['deb']
  mode '0644'
  checksum node['gperftools']['deb_sha256']
end

dpkg_package 'gperftools' do
  source '/tmp/gperftools.deb'
  version node['gperftools']['version']
  action :install
end

file '/tmp/gperftools.deb' do
  action :delete
end
