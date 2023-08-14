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

if ! platform_family?('mac_os_x')
  return
end

# Use the current user but the wheel group when creating
# resources on macOS. This avoids the need to run as sudo.
default['owner'] = node['current_user']
default['group'] = 'wheel'

default['bazel']['download_path'] =
  "https://github.com/bazelbuild/bazel/releases/download/6.2.0/bazel-6.2.0-darwin-x86_64"
default['bazel']['sha256'] =
  'd2356012843ce3a2fbba89f88191673a6ad2f7716cc46ad43ec1bcee78d36b44'

default['codecov']['download_path'] =
  'https://uploader.codecov.io/v0.2.3/macos/codecov'
default['codecov']['sha256'] =
  '8d3709d957c7115610e764621569728be102d213fee15bc1d1aa9d465eb2c258'

default['golang']['download_path'] =
  'https://go.dev/dl/go1.21.0.darwin-amd64.tar.gz'
default['golang']['sha256'] =
  'b314de9f704ab122c077d2ec8e67e3670affe8865479d1f01991e7ac55d65e70'

default['golangci-lint']['download_path'] =
  'https://github.com/golangci/golangci-lint/releases/download/v1.51.1/golangci-lint-1.51.1-darwin-amd64.tar.gz'
default['golangci-lint']['sha256'] =
  'fba08acc4027f69f07cef48fbff70b8a7ecdfaa1c2aba9ad3fb31d60d9f5d4bc'

default['nodejs']['download_path'] =
  'https://nodejs.org/dist/v18.16.0/node-v18.16.0-darwin-x64.tar.gz'
default['nodejs']['sha256'] =
  'cd520da6e2e89fab881c66a3e9aff02cb0d61d68104b1d6a571dd71bef920870'

default['prototool']['download_path'] =
  'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Darwin-x86_64'
default['prototool']['sha256'] =
  '5ca2a19f1cb04bc5059bb07e14d565231246e623f7523fafe9389b463addf645'

default['shellcheck']['download_path'] =
  'https://github.com/koalaman/shellcheck/releases/download/v0.9.0/shellcheck-v0.9.0.darwin.x86_64.tar.xz'
default['shellcheck']['sha256'] =
  '7d3730694707605d6e60cec4efcb79a0632d61babc035aa16cda1b897536acf5'
