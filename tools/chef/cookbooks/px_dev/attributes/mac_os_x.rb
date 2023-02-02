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
  "https://github.com/bazelbuild/bazel/releases/download/6.0.0/bazel-6.0.0-darwin-x86_64"
default['bazel']['sha256'] =
  '8e543c5c9f1c8c91df945cd2fb4c3b43587929a43044a0ed87d13da0d19f96e8'

default['codecov']['download_path'] =
  'https://uploader.codecov.io/v0.2.3/macos/codecov'
default['codecov']['sha256'] =
  '8d3709d957c7115610e764621569728be102d213fee15bc1d1aa9d465eb2c258'

default['fossa']['download_path'] =
  'https://github.com/fossas/fossa-cli/releases/download/v1.1.10/fossa-cli_1.1.10_darwin_amd64.tar.gz'
default['fossa']['sha256'] =
  '39f23d382c63381ec98e0b22cbf60c2007bdb699b034bfd37692a062ba254a8d'

default['golang']['download_path'] =
  'https://dl.google.com/go/go1.20.darwin-amd64.tar.gz'
default['golang']['sha256'] =
  '777025500f62d14bb5a4923072cd97431887961d24de08433a60c2fe1120531d'

default['golangci-lint']['download_path'] =
  'https://github.com/golangci/golangci-lint/releases/download/v1.48.0/golangci-lint-1.48.0-darwin-amd64.tar.gz'
default['golangci-lint']['sha256'] =
  'ec2e1c3bb3d34268cd57baba6b631127beb185bbe8cfde8ac40ba9b4c8615784'

default['nodejs']['download_path'] =
  'https://nodejs.org/dist/v16.13.2/node-v16.13.2-darwin-x64.tar.gz'
default['nodejs']['sha256'] =
  '900a952bb77533d349e738ff8a5179a4344802af694615f36320a888b49b07e6'

default['prototool']['download_path'] =
  'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Darwin-x86_64'
default['prototool']['sha256'] =
  '5ca2a19f1cb04bc5059bb07e14d565231246e623f7523fafe9389b463addf645'

default['shellcheck']['download_path'] =
  'https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.darwin.x86_64.tar.xz'
default['shellcheck']['sha256'] =
  'c4edf1f04e53a35c39a7ef83598f2c50d36772e4cc942fb08a1114f9d48e5380'
