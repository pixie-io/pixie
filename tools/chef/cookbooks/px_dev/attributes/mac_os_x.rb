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

default['gh']['download_path'] =
  'https://github.com/cli/cli/releases/download/v2.12.1/gh_2.12.1_macOS_amd64.tar.gz'
default['gh']['sha256'] =
  '448d617c11b964cff135bab43f73b321386c09fc5cdd998a17cbfc422f54239e'

default['golang']['download_path'] =
  'https://dl.google.com/go/go1.19.4.darwin-amd64.tar.gz'
default['golang']['sha256'] =
  '44894862d996eec96ef2a39878e4e1fce4d05423fc18bdc1cbba745ebfa41253'

default['golangci-lint']['download_path'] =
  'https://github.com/golangci/golangci-lint/releases/download/v1.48.0/golangci-lint-1.48.0-darwin-amd64.tar.gz'
default['golangci-lint']['sha256'] =
  'ec2e1c3bb3d34268cd57baba6b631127beb185bbe8cfde8ac40ba9b4c8615784'

default['helm']['download_path'] =
  'https://get.helm.sh/helm-v3.5.2-darwin-amd64.tar.gz'
default['helm']['sha256'] =
  '68040e9a2f147a92c2f66ce009069826df11f9d1e1c6b78c7457066080ad3229'

default['kustomize']['download_path'] =
  'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.2.0/kustomize_3.2.0_darwin_amd64'
default['kustomize']['sha256'] =
  'c7991a79470a52a95f1fac33f588b76f64e597ac64b54106e452f3a8f642c62e'

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

default['yq']['download_path'] =
  'https://github.com/mikefarah/yq/releases/download/v4.13.4/yq_darwin_amd64'
default['yq']['sha256'] =
  '17ab1aa6589f5be6398c60acc875426f4f64faeaba6ee581f700b0a9f47da19e'
