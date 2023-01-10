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
  "https://github.com/bazelbuild/bazel/releases/download/#{default['bazel']['version']}/bazel-#{default['bazel']['version']}-linux-x86_64"
default['bazel']['sha256'] =
  'f03d44ecaac3878e3d19489e37caa4ca1dc57427b686a78a85065ea3c27ebe68'

default['codecov']['download_path'] =
  'https://uploader.codecov.io/v0.2.3/linux/codecov'
default['codecov']['sha256'] =
  '648b599397548e4bb92429eec6391374c2cbb0edb835e3b3f03d4281c011f401'

default['faq']['download_path'] =
  'https://github.com/jzelinskie/faq/releases/download/0.0.7/faq-linux-amd64'
default['faq']['sha256'] =
  '6c9234d0b2b024bf0e7c845fc092339b51b94e5addeee9612a7219cfd2a7b731'

default['fossa']['download_path'] =
  'https://github.com/fossas/fossa-cli/releases/download/v1.1.10/fossa-cli_1.1.10_linux_amd64.tar.gz'
default['fossa']['sha256'] =
  'a263aabf09308614a39d8486df722f3b03ab5b0f5060b655be1fd9def8e5619f'

default['gh']['download_path'] =
  'https://github.com/cli/cli/releases/download/v2.12.1/gh_2.12.1_linux_amd64.tar.gz'
default['gh']['sha256'] =
  '359ff9d759b67e174214098144a530a8afc4b0c9d738cd07c83ac84390cdc988'

default['golang']['download_path'] =
  'https://dl.google.com/go/go1.19.4.linux-amd64.tar.gz'
default['golang']['sha256'] =
  'c9c08f783325c4cf840a94333159cc937f05f75d36a8b307951d5bd959cf2ab8'

default['golangci-lint']['download_path'] =
  'https://github.com/golangci/golangci-lint/releases/download/v1.48.0/golangci-lint-1.48.0-linux-amd64.tar.gz'
default['golangci-lint']['sha256'] =
  '127c5c9d47cf3a3cf4128815dea1d9623d57a83a22005e91b986b0cbceb09233'

default['helm']['download_path'] =
  'https://get.helm.sh/helm-v3.5.2-linux-amd64.tar.gz'
default['helm']['sha256'] =
  '01b317c506f8b6ad60b11b1dc3f093276bb703281cb1ae01132752253ec706a2'

default['kubectl']['download_path'] =
  'https://storage.googleapis.com/kubernetes-release/release/v1.21.5/bin/linux/amd64/kubectl'
default['kubectl']['sha256'] =
  '060ede75550c63bdc84e14fcc4c8ab3017f7ffc032fc4cac3bf20d274fab1be4'

default['kustomize']['download_path'] =
  'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.2.0/kustomize_3.2.0_linux_amd64'
default['kustomize']['sha256'] =
  '7db89e32575d81393d5d84f0dc6cbe444457e61ce71af06c6e6b7b6718299c22'

default['lego']['download_path'] =
  'https://github.com/go-acme/lego/releases/download/v4.5.3/lego_v4.5.3_linux_amd64.tar.gz'
default['lego']['sha256'] =
  'd6a6dbf82ae9a1a7f9fbc8d85c224617a17337afa4284aaca6b0556a7347609d'

default['minikube']['download_path'] =
  'https://github.com/kubernetes/minikube/releases/download/v1.24.0/minikube-linux-amd64'
default['minikube']['sha256'] =
  '3bc218476cf205acf11b078d45210a4882e136d24a3cbb7d8d645408e423b8fe'

default['nodejs']['download_path'] =
  'https://nodejs.org/dist/v16.13.2/node-v16.13.2-linux-x64.tar.xz'
default['nodejs']['sha256'] =
  '7f5e9a42d6e86147867d35643c7b1680c27ccd45db85666fc52798ead5e74421'

default['opm']['download_path'] =
  'https://github.com/operator-framework/operator-registry/releases/download/v1.17.3/linux-amd64-opm'
default['opm']['sha256'] =
  '771b72d802ac58b740ac493caf79256b28686907d3578f3b1e1e77b570bda156'

default['packer']['download_path'] =
  'https://releases.hashicorp.com/packer/1.7.8/packer_1.7.8_linux_amd64.zip'
default['packer']['sha256'] =
  '8a94b84542d21b8785847f4cccc8a6da4c7be5e16d4b1a2d0a5f7ec5532faec0'

default['prototool']['download_path'] =
  'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Linux-x86_64'
default['prototool']['sha256'] =
  '2247ff34ad31fa7d9433b3310879190d1ab63b2ddbd58257d24c267f53ef64e6'

default['sentry']['download_path'] =
  'https://github.com/getsentry/sentry-cli/releases/download/1.52.0/sentry-cli-Linux-x86_64'
default['sentry']['sha256'] =
  'd6aeb45efbcdd3ec780f714b5082046ea1db31ff60ed0fc39916bbc8b6d708be'

default['shellcheck']['download_path'] =
  'https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.linux.x86_64.tar.xz'
default['shellcheck']['sha256'] =
  '39c501aaca6aae3f3c7fc125b3c3af779ddbe4e67e4ebdc44c2ae5cba76c847f'

default['skaffold']['download_path'] =
  'https://storage.googleapis.com/skaffold/releases/v1.38.0/skaffold-linux-amd64'
default['skaffold']['sha256'] =
  '3c347c9478880f22ebf95807c13371844769c625cf3ea9c987cd85859067503c'

default['sops']['download_path'] =
  'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.linux'
default['sops']['sha256'] =
  '6eacdd01b68fd140eb71bbca233bea897cccb75dbf9e00a02e648b2f9a8a6939'

default['yq']['download_path'] =
  'https://github.com/mikefarah/yq/releases/download/v4.13.4/yq_linux_amd64'
default['yq']['sha256'] =
  '11092943c548232bc670504303807e5f4b68adc9690fae74069c1c7f5dff0f3f'
