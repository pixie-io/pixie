# Install Java.
apt_repository 'oracle-java' do
  uri         'ppa:webupd8team/java'
end

apt_update 'update packages' do
  action :update
end

execute 'java-selection' do
  command 'echo oracle-java8-installer shared/accepted-oracle-license-v1-1 select true |'\
          ' /usr/bin/debconf-set-selections'
  action :run
end

apt_package 'oracle-java8-installer' do
  action :upgrade
end
