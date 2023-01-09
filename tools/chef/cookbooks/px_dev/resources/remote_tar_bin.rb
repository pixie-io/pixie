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

unified_mode true
provides :remote_tar_bin

property :name, String, name_property: true
property :bin_dir, String, default: '/opt/pixielabs/bin'
property :strip_components, Integer, default: 0

default_action :create

action :create do
  archive_path = "/tmp/#{new_resource.name}.tar.gz"

  remote_file archive_path do
    source node[new_resource.name]['download_path']
    mode 0644
    checksum node[new_resource.name]['sha256']
  end

  flags = []
  flags << "-C #{new_resource.bin_dir}"
  if new_resource.strip_components > 0
    flags << "--strip-components #{new_resource.strip_components}"
  end
  flags << '--no-anchored'

  execute "install #{new_resource.name}" do
    command "tar xf #{archive_path} #{flags.join(' ')} #{new_resource.name}"
  end

  file archive_path do
    action :delete
  end
end
