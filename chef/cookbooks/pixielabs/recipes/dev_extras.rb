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
