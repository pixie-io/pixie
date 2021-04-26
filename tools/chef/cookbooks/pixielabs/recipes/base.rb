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

ENV['PATH'] = "/opt/gsutil:#{ENV['PATH']}"

case node['platform']
when 'mac_os_x'
  include_recipe 'pixielabs::mac_os_x'
  root_group = 'wheel'
  user = node['current_user']
else
  include_recipe 'pixielabs::linux'
  root_group = 'root'
  user = 'root'
end

execute 'install_python_packages' do
  command 'python3 -m pip install flake8 flake8-mypy setuptools yamllint numpy'
end

include_recipe 'pixielabs::phabricator'
include_recipe 'pixielabs::nodejs'
include_recipe 'pixielabs::golang'

execute 'install node packages' do
  command %(/opt/node/bin/npm install -g \
            tslint@5.11.0 typescript@3.0.1 yarn@1.22.4 webpack@4.42.0 \
            jshint@2.11.0 jest@23.4.2 lsif-tsc@0.5.6)
end

directory '/opt/pixielabs' do
  owner user
  group root_group
  mode '0755'
  action :create
end

directory '/opt/pixielabs/bin' do
  owner user
  group root_group
  mode '0755'
  action :create
end

template '/opt/pixielabs/plenv.inc' do
  source 'plenv.inc.erb'
  owner user
  group root_group
  mode '0644'
  action :create
end

template '/opt/pixielabs/bin/tot' do
  source 'tot.erb'
  owner user
  group root_group
  mode '0755'
  action :create
end

remote_file '/opt/pixielabs/bin/bazel' do
  source node['bazel']['download_path']
  mode 0555
  checksum node['bazel']['sha256']
end

remote_file '/opt/pixielabs/bin/kustomize' do
  source node['kustomize']['download_path']
  mode 0755
  checksum node['kustomize']['sha256']
end

remote_file '/opt/pixielabs/bin/sops' do
  source node['sops']['download_path']
  mode 0755
  checksum node['sops']['sha256']
end

remote_file '/tmp/shellcheck.tar.xz' do
  source node['shellcheck']['download_path']
  mode 0755
  checksum node['shellcheck']['sha256']
end

execute 'install shellcheck' do
  command 'tar xf /tmp/shellcheck.tar.xz -C /opt/pixielabs/bin --strip-components 1'
end

file '/tmp/shellcheck.tar.xz' do
  action :delete
end


remote_file '/opt/pixielabs/bin/prototool' do
  source node['prototool']['download_path']
  mode 0755
  checksum node['prototool']['sha256']
end

remote_file '/opt/pixielabs/bin/yq' do
  source node['yq']['download_path']
  mode 0755
  checksum node['yq']['sha256']
end

remote_file '/opt/pixielabs/bin/src' do
  source node['src']['download_path']
  mode 0755
  checksum node['src']['sha256']
end

remote_file '/opt/pixielabs/bin/lsif-go' do
  source node['lsif-go']['download_path']
  mode 0755
  checksum node['lsif-go']['sha256']
end

remote_file '/tmp/golangci-lint.tar.gz' do
  source node['golangci-lint']['download_path']
  mode 0755
  checksum node['golangci-lint']['sha256']
end

execute 'install golangci-lint' do
  command 'tar xf /tmp/golangci-lint.tar.gz -C /opt/pixielabs/bin --strip-components 1'
end

file '/tmp/golangci-lint.tar.gz' do
  action :delete
end

remote_file '/tmp/gsutil.tar.gz' do
  source node['gsutil']['download_path']
  mode 0755
  checksum node['gsutil']['sha256']
end

execute 'install gsutil' do
  command 'tar xf /tmp/gsutil.tar.gz -C /opt'
end

file '/tmp/gsutil.tar.gz' do
  action :delete
end

remote_file '/tmp/helm.tar.gz' do
  source node['helm']['download_path']
  mode 0755
  checksum node['helm']['sha256']
end

execute 'install helm' do
  command 'tar xf /tmp/helm.tar.gz -C /opt/pixielabs/bin --strip-components 1'
end

file '/tmp/helm.tar.gz' do
  action :delete
end
