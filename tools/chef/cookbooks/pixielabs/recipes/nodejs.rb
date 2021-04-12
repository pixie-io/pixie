directory '/opt/node' do
  mode '0755'
  action :create
end

remote_file '/tmp/nodejs.tar.gz' do
  source node['nodejs']['download_path']
  mode 0644
  checksum node['nodejs']['sha256']
end

execute 'install_node' do
   command 'tar xf /tmp/nodejs.tar.gz -C /opt/node --strip-components 1'
   action :run
 end

file '/tmp/nodejs.tar.gz' do
  action :delete
end

ENV['PATH'] = "/opt/node/bin:#{ENV['PATH']}"
