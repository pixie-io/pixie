# The contains additional setup required in our docker image,
# so that we can mount our code and use Golang.
ENV['GOPATH'] = '/pl'
ENV['PATH'] = "#{ENV['GOPATH']}/bin:/usr/lib/go-1.10/bin/:#{ENV['PATH']}"

directory '/pl' do
  mode '0755'
  action :create
end

execute 'Install golint' do
  command 'go get -u golang.org/x/lint/golint'
end

execute 'Install goimports' do
  command 'go get -u golang.org/x/tools/cmd/goimports'
end
