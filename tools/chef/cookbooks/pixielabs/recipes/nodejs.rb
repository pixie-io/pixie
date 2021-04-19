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

directory '/opt/node' do
  mode '0755'
  action :create
end

remote_file '/tmp/nodejs.tar.gz' do
  source node['nodejs']['download_path']
  mode 0644
  checksum node['nodejs']['sha256']
end

execute 'install_node' do
   command 'tar xf /tmp/nodejs.tar.gz -C /opt/node --strip-components 1'
   action :run
 end

file '/tmp/nodejs.tar.gz' do
  action :delete
end

ENV['PATH'] = "/opt/node/bin:#{ENV['PATH']}"
