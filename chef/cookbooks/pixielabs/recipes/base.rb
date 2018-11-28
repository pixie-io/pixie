case node['platform']
when 'mac_os_x'
  include_recipe 'pixielabs::mac_os_x'
  root_group = 'wheel'
else
  include_recipe 'pixielabs::linux'
  root_group = 'root'
end

execute 'install_python_packages' do
  command 'pip3 install flake8 flake8-mypy setuptools'
end

include_recipe 'pixielabs::phabricator'
include_recipe 'pixielabs::nodejs'

execute 'install node packages' do
  command 'npm install -g tslint@5.11.0 typescript@3.0.1 yarn@1.9.4 webpack@4.16.5 jshint@2.9.6 jest@23.4.2'
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
