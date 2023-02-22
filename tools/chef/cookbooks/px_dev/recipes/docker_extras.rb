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


# This overrides the ENV declared in our go install. This means that when
# this recipe is included, go tools end up in the location specified here (if
# this recipe is included after the base recipe).
ENV['GOPATH'] = '/px'
ENV['PATH'] = "#{ENV['GOPATH']}/bin:#{ENV['PATH']}"

execute 'mark expected src dir as safe' do
  command 'git config --global --add safe.directory /px/src/px.dev/pixie'
end

execute 'link iptables into /usr/bin' do
  command 'ln -s /usr/sbin/iptables /usr/bin/iptables && ln -s /usr/sbin/ip6tables /usr/bin/ip6tables'
end

execute 'link gcr credential helper into /usr/bin' do
  command 'ln -s /opt/google-cloud-sdk/bin/docker-credential-gcr /usr/bin/docker-credential-gcr'
end

directory '/etc/containers' do
  owner node['owner']
  group node['group']
  mode '0755'
  action :create
end

template '/etc/containers/containers.conf' do
  source 'containers.conf.erb'
  owner node['owner']
  group node['group']
  mode '0644'
  action :create
end
