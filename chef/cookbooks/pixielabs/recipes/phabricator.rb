case node['platform']
when 'mac_os_x'
  root_group = 'wheel'
  user = node['current_user']
else
  root_group = 'root'
  user = 'root'
end

directory '/opt/phab' do
  owner user
  group root_group
  mode '0755'
  action :create
end

git '/opt/phab/arcanist' do
  repository 'https://github.com/phacility/arcanist.git'
  checkout_branch 'stable'
  action :sync
end
ENV['PATH'] = "/opt/phab/arcanist/bin:#{ENV['PATH']}"
