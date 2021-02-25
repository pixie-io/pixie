ENV['PATH'] = "/opt/gsutil:#{ENV['PATH']}"

case node['platform']
when 'mac_os_x'
  include_recipe 'pixielabs::mac_os_x'
  root_group = 'wheel'
else
  include_recipe 'pixielabs::linux'
  root_group = 'root'
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
            jshint@2.11.0 jest@23.4.2 @sourcegraph/lsif-tsc@0.6.6)
end

directory '/opt/pixielabs' do
  owner 'root'
  group root_group
  mode '0755'
  action :create
end

directory '/opt/pixielabs/bin' do
  owner 'root'
  group root_group
  mode '0755'
  action :create
end

template '/opt/pixielabs/plenv.inc' do
  source 'plenv.inc.erb'
  owner 'root'
  group root_group
  mode '0644'
  action :create
end

template '/opt/pixielabs/bin/tot' do
  source 'tot.erb'
  owner 'root'
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

ark 'shellcheck' do
  url node['shellcheck']['download_path']
  has_binaries ['shellcheck']
  checksum node['shellcheck']['sha256']
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

remote_file '/tmp/gsutil.tar.gz' do
  source node['gsutil']['download_path']
  mode 0755
  checksum node['gsutil']['sha256']
end

execute 'install gsutil' do
  command 'tar xfz /tmp/gsutil.tar.gz -C /opt'
end

file '/tmp/gsutil.tar.gz' do
  action :delete
end

remote_file '/tmp/helm.tar.gz' do
  source node['helm']['download_path']
  mode 0755
  checksum node['helm']['sha256']
end

directory '/tmp/helm' do
  owner 'root'
  group root_group
  mode '0755'
  action :create
end

execute 'install helm' do
  command 'tar xfz /tmp/helm.tar.gz -C /opt/pixielabs/bin --strip-components 1'
end

file '/tmp/helm.tar.gz' do
  action :delete
end
