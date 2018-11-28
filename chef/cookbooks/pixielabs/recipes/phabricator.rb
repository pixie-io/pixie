directory '/opt/phab' do
  owner 'root'
  mode '0755'
  action :create
end

git '/opt/phab/libphutil' do
  repository 'https://github.com/phacility/libphutil.git'
  action :sync
end

git '/opt/phab/arcanist' do
  repository 'https://github.com/phacility/arcanist.git'
  action :sync
end
ENV['PATH'] = "/opt/phab/arcanist/bin:#{ENV['PATH']}"
