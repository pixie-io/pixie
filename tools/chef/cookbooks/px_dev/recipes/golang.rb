# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

directory '/opt/px_dev/tools/golang' do
  owner node['owner']
  group node['group']
  mode '0755'
  recursive true
  action [:delete, :create]
end

remote_file '/tmp/golang.tar.gz' do
  source node['golang']['download_path']
  mode '0644'
  checksum node['golang']['sha256']
end

execute 'install_golang' do
   command 'tar xf /tmp/golang.tar.gz -C /opt/px_dev/tools/golang --strip-components 1'
   action :run
 end

file '/tmp/golang.tar.gz' do
  action :delete
end

ENV['PATH'] = "/opt/px_dev/tools/golang/bin:#{ENV['PATH']}"

execute 'install go binaries' do
  ENV['GOPATH'] = "/opt/px_dev/gopath"
  command %(go install github.com/golang/mock/mockgen@v1.5.0 && \
            go install github.com/cheekybits/genny@v1.0.0 && \
            go install sigs.k8s.io/controller-tools/cmd/controller-gen@v0.4.1 && \
            go install k8s.io/code-generator/cmd/client-gen@v0.20.6 && \
            go install github.com/go-bindata/go-bindata/go-bindata@v3.1.2+incompatible && \
            go install github.com/google/go-containerregistry/cmd/crane@v0.15.2 && \
            go install github.com/sigstore/cosign/v2/cmd/cosign@v2.0.2 && \
            go install github.com/regclient/regclient/cmd/regctl@v0.4.8 && \
            go install github.com/regclient/regclient/cmd/regsync@v0.4.8 && \
            go install github.com/regclient/regclient/cmd/regbot@v0.4.8 && \
            go clean -modcache && \
            go clean -cache)
end
