homebrew_package 'autoconf'
homebrew_package 'automake'
homebrew_package 'checkstyle'
homebrew_package 'clang-format'
homebrew_package 'libtool'
homebrew_package 'postgresql'
homebrew_package 'python'
homebrew_package 'python3'

homebrew_package 'pyenv-virtualenv'

homebrew_cask 'docker-edge'

execute "install pip" do
  command "/usr/bin/easy_install pip"
  creates "/usr/local/bin/pip"
  not_if { ::File.exist?("/usr/local/bin/pip") }
end
