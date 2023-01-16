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

remote_file '/tmp/gcc_musl.deb' do
  source node['gcc-musl']['deb']
  mode '0644'
  checksum node['gcc-musl']['deb_sha256']
end

dpkg_package 'gcc-musl' do
  source '/tmp/gcc_musl.deb'
  action :install
  version node['gcc-musl']['version']
end

file '/tmp/gcc_musl.deb' do
  action :delete
end
