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
  "https://github.com/bazelbuild/bazel/releases/download/#{default['bazel']['version']}/bazel-#{default['bazel']['version']}-darwin-x86_64"
default['bazel']['sha256'] =
  '8e543c5c9f1c8c91df945cd2fb4c3b43587929a43044a0ed87d13da0d19f96e8'

default['codecov']['download_path'] =
  'https://uploader.codecov.io/v0.2.3/macos/codecov'
default['codecov']['sha256'] =
  '8d3709d957c7115610e764621569728be102d213fee15bc1d1aa9d465eb2c258'

default['faq']['download_path'] =
  'https://github.com/jzelinskie/faq/releases/download/0.0.7/faq-darwin-amd64'
default['faq']['sha256'] =
  '869f4d8acaa1feb11ce76b2204c5476b8a04d9451216adde6b18e2ef2f978794'

default['fossa']['download_path'] =
  'https://github.com/fossas/fossa-cli/releases/download/v1.1.10/fossa-cli_1.1.10_darwin_amd64.tar.gz'
default['fossa']['sha256'] =
  '39f23d382c63381ec98e0b22cbf60c2007bdb699b034bfd37692a062ba254a8d'

default['golang']['download_path'] =
  'https://dl.google.com/go/go1.19.4.darwin-amd64.tar.gz'
default['golang']['sha256'] =
  '44894862d996eec96ef2a39878e4e1fce4d05423fc18bdc1cbba745ebfa41253'

default['gh']['download_path'] =
  'https://github.com/cli/cli/releases/download/v2.12.1/gh_2.12.1_macOS_amd64.tar.gz'
default['gh']['sha256'] =
  '448d617c11b964cff135bab43f73b321386c09fc5cdd998a17cbfc422f54239e'

default['golangci-lint']['download_path'] =
  'https://github.com/golangci/golangci-lint/releases/download/v1.48.0/golangci-lint-1.48.0-darwin-amd64.tar.gz'
default['golangci-lint']['sha256'] =
  'ec2e1c3bb3d34268cd57baba6b631127beb185bbe8cfde8ac40ba9b4c8615784'

default['helm']['download_path'] =
  'https://get.helm.sh/helm-v3.5.2-darwin-amd64.tar.gz'
default['helm']['sha256'] =
  '68040e9a2f147a92c2f66ce009069826df11f9d1e1c6b78c7457066080ad3229'

default['kubectl']['download_path'] =
  'https://storage.googleapis.com/kubernetes-release/release/v1.26.0/bin/darwin/amd64/kubectl'
default['kubectl']['sha256'] =
  'be9dc0782a7b257d9cfd66b76f91081e80f57742f61e12cd29068b213ee48abc'

default['kustomize']['download_path'] =
  'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.2.0/kustomize_3.2.0_darwin_amd64'
default['kustomize']['sha256'] =
  'c7991a79470a52a95f1fac33f588b76f64e597ac64b54106e452f3a8f642c62e'

default['lego']['download_path'] =
  'https://github.com/go-acme/lego/releases/download/v4.5.3/lego_v4.5.3_darwin_amd64.tar.gz'
default['lego']['sha256'] =
  'eaf2792d9731c911da671a6145eebd5ba136c20446adb542e7b1463ffe868388'

default['minikube']['download_path'] =
  'https://github.com/kubernetes/minikube/releases/download/v1.24.0/minikube-darwin-amd64'
default['minikube']['sha256'] =
  '55f14e4f411370da18d7b9432ae4edd128f4f047bbc87a278e08ba256ff6f669'

default['nodejs']['download_path'] =
  'https://nodejs.org/dist/v16.13.2/node-v16.13.2-darwin-x64.tar.gz'
default['nodejs']['sha256'] =
  '900a952bb77533d349e738ff8a5179a4344802af694615f36320a888b49b07e6'

default['opm']['download_path'] =
  'https://github.com/operator-framework/operator-registry/releases/download/v1.17.3/darwin-amd64-opm'
default['opm']['sha256'] =
  'bb812b97fb3c65f634ecc26910b2dfc0f2e668b86272702200937937df83ad5a'

default['packer']['download_path'] =
  'https://releases.hashicorp.com/packer/1.7.8/packer_1.7.8_darwin_amd64.zip'
default['packer']['sha256'] =
  'f8abe5d8660be2e6bea04bbb165ede4026e66f2f48ae5f076f9ea858699357ae'

default['prototool']['download_path'] =
  'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Darwin-x86_64'
default['prototool']['sha256'] =
  '5ca2a19f1cb04bc5059bb07e14d565231246e623f7523fafe9389b463addf645'

default['sentry']['download_path'] =
  'https://github.com/getsentry/sentry-cli/releases/download/1.52.0/sentry-cli-Darwin-x86_64'
default['sentry']['sha256'] =
  '97c9bafbcf87bd7dea4f1069fe18f8e8265de6f7eab20c62ca9299e0fa8c2af6'

default['shellcheck']['download_path'] =
  'https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.darwin.x86_64.tar.xz'
default['shellcheck']['sha256'] =
  'c4edf1f04e53a35c39a7ef83598f2c50d36772e4cc942fb08a1114f9d48e5380'

default['skaffold']['download_path'] =
  'https://storage.googleapis.com/skaffold/releases/v1.38.0/skaffold-darwin-amd64'
default['skaffold']['sha256'] =
  '872897d78a17812913cd6e930c5d1c94f7c862381db820815c4bffc637c28b88'

default['sops']['download_path'] =
  'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.darwin'
default['sops']['sha256'] =
  '09bb5920ae609bdf041b74843e2d8211a7059847b21729fadfbd3c3e33e67d26'

default['yq']['download_path'] =
  'https://github.com/mikefarah/yq/releases/download/v4.13.4/yq_darwin_amd64'
default['yq']['sha256'] =
  '17ab1aa6589f5be6398c60acc875426f4f64faeaba6ee581f700b0a9f47da19e'
