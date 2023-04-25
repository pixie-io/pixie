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

if ! platform_family?('debian')
  return
end

# Resources created by chef on linux are owned by root:root
default['owner'] = 'root'
default['group'] = 'root'

default['bazel']['download_path'] =
  "https://github.com/bazelbuild/bazel/releases/download/6.0.0/bazel-6.0.0-linux-x86_64"
default['bazel']['sha256'] =
  'f03d44ecaac3878e3d19489e37caa4ca1dc57427b686a78a85065ea3c27ebe68'

default['codecov']['download_path'] =
  'https://uploader.codecov.io/v0.2.3/linux/codecov'
default['codecov']['sha256'] =
  '648b599397548e4bb92429eec6391374c2cbb0edb835e3b3f03d4281c011f401'

default['golang']['download_path'] =
  'https://go.dev/dl/go1.20.2.linux-amd64.tar.gz'
default['golang']['sha256'] =
  '4eaea32f59cde4dc635fbc42161031d13e1c780b87097f4b4234cfce671f1768'

default['golangci-lint']['download_path'] =
  'https://github.com/golangci/golangci-lint/releases/download/v1.51.1/golangci-lint-1.51.1-linux-amd64.tar.gz'
default['golangci-lint']['sha256'] =
  '17aeb26c76820c22efa0e1838b0ab93e90cfedef43fbfc9a2f33f27eb9e5e070'

default['nodejs']['download_path'] =
  'https://nodejs.org/dist/v16.13.2/node-v16.13.2-linux-x64.tar.xz'
default['nodejs']['sha256'] =
  '7f5e9a42d6e86147867d35643c7b1680c27ccd45db85666fc52798ead5e74421'

default['prototool']['download_path'] =
  'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Linux-x86_64'
default['prototool']['sha256'] =
  '2247ff34ad31fa7d9433b3310879190d1ab63b2ddbd58257d24c267f53ef64e6'

default['shellcheck']['download_path'] =
  'https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.linux.x86_64.tar.xz'
default['shellcheck']['sha256'] =
  '39c501aaca6aae3f3c7fc125b3c3af779ddbe4e67e4ebdc44c2ae5cba76c847f'
