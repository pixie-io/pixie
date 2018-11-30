remote_file '/tmp/bcc.deb' do
  source node['bcc']['deb']
  mode 0644
  checksum node['bcc']['deb_sha256']
end

dpkg_package 'bcc' do
  source '/tmp/bcc.deb'
  action :install
end

file '/tmp/bcc.deb' do
  action :delete
end
