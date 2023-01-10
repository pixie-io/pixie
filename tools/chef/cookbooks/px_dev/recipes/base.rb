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

include_recipe 'px_dev::setup'

if platform_family?('debian')
  include_recipe 'px_dev::linux'
end

if platform_family?('mac_os_x')
  include_recipe 'px_dev::mac_os_x'
end

execute 'install_python_packages' do
  command 'python3 -m pip install flake8 flake8-mypy yamllint --no-cache-dir'
  command 'python3 -m pip cache purge'
end

include_recipe 'px_dev::phabricator'
include_recipe 'px_dev::nodejs'
include_recipe 'px_dev::golang'

execute 'install node packages' do
  command %(/opt/node/bin/npm install -g \
            jshint@2.11.0 yarn@1.22.4 protobufjs@6.11.2)
end

execute 'install pbjs/pbts deps' do
  command '/opt/node/bin/pbjs || true'
end

execute 'install go binaries' do
  ENV['GOPATH'] = "/opt/pixielabs/gopath"
  command %(go install golang.org/x/lint/golint@v0.0.0-20210508222113-6edffad5e616 && \
            go install golang.org/x/tools/cmd/goimports@v0.1.2 && \
            go install github.com/golang/mock/mockgen@v1.5.0 && \
            go install github.com/cheekybits/genny@v1.0.0 && \
            go install sigs.k8s.io/controller-tools/cmd/controller-gen@v0.4.1 && \
            go install k8s.io/code-generator/cmd/client-gen@v0.20.6 && \
            go install github.com/go-bindata/go-bindata/go-bindata@v3.1.2+incompatible && \
            go clean -cache)
end

template '/opt/pixielabs/plenv.inc' do
  source 'plenv.inc.erb'
  owner node['owner']
  group node['group']
  mode '0644'
  action :create
end

template '/opt/pixielabs/bin/bazel' do
  source 'bazel.erb'
  owner node['owner']
  group node['group']
  mode '0755'
  action :create
end

remote_bin 'bazel' do
  bin_name 'bazel_core'
end

remote_bin 'codecov'
remote_bin 'faq'
remote_bin 'kustomize'
remote_bin 'opm'
remote_bin 'prototool'
remote_bin 'yq'

remote_tar_bin 'fossa'

remote_tar_bin 'gh' do
  tool_loc 'bin/gh'
  strip_components 1
end

remote_tar_bin 'golangci-lint' do
  strip_components 1
end

remote_tar_bin 'gsutil' do
  strip_components 2
end

remote_tar_bin 'helm' do
  strip_components 1
end

remote_tar_bin 'shellcheck' do
  strip_components 1
end

