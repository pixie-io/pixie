homebrew_package 'automake'
homebrew_package 'autoconf'
homebrew_package 'python'
homebrew_package 'python3'
homebrew_package 'go'
homebrew_package 'postgresql'
homebrew_package 'pyenv-virtualenv'
homebrew_package 'clang-format'
homebrew_package 'dep'
homebrew_package 'libtool'

homebrew_cask 'docker-edge'

execute "install pip" do
  command "/usr/bin/easy_install pip"
  creates "/usr/local/bin/pip"
  not_if { ::File.exist?("/usr/local/bin/pip") }
end

remote_file '/usr/local/bin/bazel' do
  source node['bazel']['download_path']
  mode 0555
  checksum node['bazel']['sha256']
end
