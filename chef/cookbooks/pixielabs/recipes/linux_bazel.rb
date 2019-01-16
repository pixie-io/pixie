remote_file '/tmp/bazel.deb' do
  source node['bazel']['deb']
  mode 0644
  checksum node['bazel']['deb_sha256']
end

dpkg_package 'bazel' do
  source '/tmp/bazel.deb'
  version node['bazel']['version']
  action :install
end

file '/tmp/bazel.deb' do
  action :delete
end
