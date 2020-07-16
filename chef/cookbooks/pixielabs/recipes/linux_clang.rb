remote_file '/tmp/clang.deb' do
  source node['clang']['deb']
  mode 0644
  checksum node['clang']['deb_sha256']
end

dpkg_package 'clang' do
  source '/tmp/clang.deb'
  action :install
  version node['clang']['version']
end

file '/tmp/clang.deb' do
  action :delete
end

ENV['PATH'] = "/opt/clang-10.0/bin:#{ENV['PATH']}"
ENV['LD_LIBRARY_PATH'] = "/opt/clang-10.0/lib:#{ENV['LD_LIBRARY_PATH']}"
ENV['CC'] = "clang"
ENV['CXX'] = "clang++"
