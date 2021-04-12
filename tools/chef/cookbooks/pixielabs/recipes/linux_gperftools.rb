remote_file '/tmp/gperftools.deb' do
  source node['gperftools']['deb']
  mode 0644
  checksum node['gperftools']['deb_sha256']
end

dpkg_package 'gperftools' do
  source '/tmp/gperftools.deb'
  version node['gperftools']['version']
  action :install
end

file '/tmp/gperftools.deb' do
  action :delete
end
