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
  "https://github.com/bazelbuild/bazel/releases/download/6.2.0/bazel-6.2.0-linux-x86_64"
default['bazel']['sha256'] =
  '3d11c26fb9ba12c833844450bb90165b176e8a19cb5cf5923f3cec855837f17c'

default['codecov']['download_path'] =
  'https://uploader.codecov.io/v0.2.3/linux/codecov'
default['codecov']['sha256'] =
  '648b599397548e4bb92429eec6391374c2cbb0edb835e3b3f03d4281c011f401'

default['golang']['download_path'] =
  'https://go.dev/dl/go1.24.4.linux-amd64.tar.gz'
default['golang']['sha256'] =
  '77e5da33bb72aeaef1ba4418b6fe511bc4d041873cbf82e5aa6318740df98717'

default['golangci-lint']['download_path'] =
  'https://github.com/golangci/golangci-lint/releases/download/v2.0.2/golangci-lint-2.0.2-linux-amd64.tar.gz'
default['golangci-lint']['sha256'] =
  '89cc8a7810dc63b9a37900da03e37c3601caf46d42265d774e0f1a5d883d53e2'

default['nodejs']['download_path'] =
  'https://nodejs.org/dist/v18.16.0/node-v18.16.0-linux-x64.tar.xz'
default['nodejs']['sha256'] =
  '44d93d9b4627fe5ae343012d855491d62c7381b236c347f7666a7ad070f26548'

default['prototool']['download_path'] =
  'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Linux-x86_64'
default['prototool']['sha256'] =
  '2247ff34ad31fa7d9433b3310879190d1ab63b2ddbd58257d24c267f53ef64e6'

default['shellcheck']['download_path'] =
  'https://github.com/koalaman/shellcheck/releases/download/v0.9.0/shellcheck-v0.9.0.linux.x86_64.tar.xz'
default['shellcheck']['sha256'] =
  '700324c6dd0ebea0117591c6cc9d7350d9c7c5c287acbad7630fa17b1d4d9e2f'
