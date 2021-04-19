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

ENV['CLOUDSDK_CORE_DISABLE_PROMPTS'] = '1'
ENV['CLOUDSDK_INSTALL_DIR'] = '/opt'
ENV['PATH'] = "/opt/google-cloud-sdk/bin:#{ENV['PATH']}"

if node[:platform] == 'ubuntu'
  apt_pkg_list = [
    'emacs',
    'jq',
    'vim',
    'zsh',
  ]

  apt_package apt_pkg_list do
    action :upgrade
  end

  include_recipe 'pixielabs::linux_gperftools'
elsif node[:platform] == 'mac_os_x'
  homebrew_package 'emacs'
  homebrew_package 'vim'
  homebrew_package 'jq'
  homebrew_package 'gperftools'
end

execute 'install gcloud' do
  command 'curl https://sdk.cloud.google.com | bash'
  creates '/opt/google-cloud-sdk'
  action :run
end

remote_file '/opt/pixielabs/bin/kubectl' do
  source node['kubectl']['download_path']
  mode 0755
  checksum node['kubectl']['sha256']
end

execute 'update gcloud' do
  command 'gcloud components update'
  action :run
end

execute 'install gcloud::beta' do
  command 'gcloud components install beta'
  action :run
end

execute 'install gcloud::docker-credential-gcr' do
  command 'gcloud components install docker-credential-gcr'
  action :run
end

execute 'configure docker-credential-gcr' do
  command 'docker-credential-gcr configure-docker'
  action :run
end

remote_file '/usr/local/bin/skaffold' do
  source node['skaffold']['download_path']
  mode 0755
  checksum node['skaffold']['sha256']
end

remote_file '/usr/local/bin/minikube' do
  source node['minikube']['download_path']
  mode 0755
  checksum node['minikube']['sha256']
end

remote_file '/tmp/packer.zip' do
  source node['packer']['download_path']
  mode 0644
  checksum node['packer']['sha256']
end

execute 'install packer' do
  command 'unzip -d /usr/local/bin -o /tmp/packer.zip && chmod +x /usr/local/bin/packer'
end

file '/tmp/packer.zip' do
  action :delete
end
