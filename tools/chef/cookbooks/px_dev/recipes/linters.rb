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

execute 'install go linters' do
  command %(go install golang.org/x/lint/golint@v0.0.0-20210508222113-6edffad5e616 && \
            go install golang.org/x/tools/cmd/goimports@v0.1.2 && \
            go clean -modcache && \
            go clean -cache)
end

execute 'install js linters' do
  command 'npm install -g jshint@2.11.0 && npm cache clean --force'
end

execute 'install py linters' do
  command 'python3 -m pip install flake8 flake8-mypy yamllint --no-cache-dir && python3 -m pip cache purge'
end

common_remote_bin 'prototool'

common_remote_tar_bin 'golangci-lint' do
  strip_components 1
end

common_remote_tar_bin 'shellcheck' do
  strip_components 1
end

template '/opt/px_dev/bin/bazel' do
  source 'bazel.erb'
  owner node['owner']
  group node['group']
  mode '0755'
  action :create
end

common_remote_bin 'bazel' do
  bin_name 'bazel_core'
end

if platform_family?('debian')
  remote_file '/tmp/clang-linters.deb' do
    source node['clang-linters']['deb']
    mode '0644'
    checksum node['clang-linters']['deb_sha256']
  end

  dpkg_package 'clang-linters' do
    source '/tmp/clang-linters.deb'
    action :install
    version node['clang-linters']['version']
  end

  file '/tmp/clang-linters.deb' do
    action :delete
  end
end
