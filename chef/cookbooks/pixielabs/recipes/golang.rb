directory '/opt/golang' do
  recursive true
  action :delete
end

directory '/opt/golang' do
  mode '0755'
  action :create
end

remote_file '/tmp/golang.tar.gz' do
  source node['golang']['download_path']
  mode 0644
  checksum node['golang']['sha256']
end

execute 'install_golang' do
   command 'tar xf /tmp/golang.tar.gz -C /opt/golang --strip-components 1'
   action :run
 end

file '/tmp/golang.tar.gz' do
  action :delete
end

ENV['PATH'] = "/opt/golang/bin:#{ENV['PATH']}"
