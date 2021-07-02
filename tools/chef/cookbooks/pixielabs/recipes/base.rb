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
            jshint@2.11.0 yarn@1.22.4 @sourcegraph/lsif-tsc@0.6.8 protobufjs@6.11.2)
end

execute 'install pbjs/pbts deps' do
  command '/opt/node/bin/pbjs || true'
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

directory '/opt/pixielabs/gopath' do
  owner user
  group root_group
  mode '0755'
  action :create
end

execute 'install go binaries' do
  ENV['GOPATH'] = "/opt/pixielabs/gopath"
  command %(go get \
            golang.org/x/lint/golint@v0.0.0-20210508222113-6edffad5e616 \
            golang.org/x/tools/cmd/goimports@v0.1.2 \
            github.com/golang/mock/mockgen@v1.5.0 \
            github.com/cheekybits/genny@v1.0.0 \
            sigs.k8s.io/controller-tools/cmd/controller-gen@v0.4.1 \
            github.com/go-bindata/go-bindata/go-bindata@v3.1.2+incompatible)
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

directory '/opt/antlr' do
  owner user
  group root_group
  mode '0755'
  action :create
end

remote_file '/opt/antlr/antlr-4.9-complete.jar' do
  source node['antlr']['download_path']
  mode 0644
  checksum node['antlr']['sha256']
end

remote_file '/opt/pixielabs/bin/opm' do
  source node['opm']['download_path']
  mode 0755
  checksum node['opm']['sha256']
end

remote_file '/opt/pixielabs/bin/faq' do
  source node['faq']['download_path']
  mode 0755
  checksum node['faq']['sha256']
end