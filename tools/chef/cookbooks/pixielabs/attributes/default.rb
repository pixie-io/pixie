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

default['clang']               = {}
default['clang']['deb']        =
  'https://storage.googleapis.com/pixie-dev-public/clang/15.0-pl2/clang-15.0-pl2.deb'
default['clang']['deb_sha256'] =
  '32549362100f3759a0b50b3e1993f58086568696839bbdd88c11602bd3e994e2'
default['clang']['version']    = "15.0-pl2"

default['gperftools']               = {}
default['gperftools']['deb']        =
  'https://storage.googleapis.com/pixie-dev-public/gperftools-pixie-2.7-pl2.deb'
default['gperftools']['deb_sha256'] =
  '10f00d6ddde920c74c80eb966362fa234c1d97349ba186b51d05bb98e3fff72e'
default['gperftools']['version']    = "2.7-pl2"

default['gsutil']                  = {}
default['gsutil']['download_path'] = 'https://storage.googleapis.com/pub/gsutil_5.17.tar.gz'
default['gsutil']['sha256']        = 'cd9495eb0437e47210c19087bf0e81c72b669102193132e5c0d72a807cc27d55'

default['antlr'] = {}
default['antlr']['download_path'] = 'https://www.antlr.org/download/antlr-4.9-complete.jar'
default['antlr']['sha256'] = 'bd11b2464bc8aee5f51b119dff617101b77fa729540ee7f08241a6a672e6bc81'

default['bazel']                     = {}
default['bazel']['version']          = '6.0.0'
default['bazel']['zsh_completions']  =
  "https://raw.githubusercontent.com/bazelbuild/bazel/#{default['bazel']['version']}/scripts/zsh_completion/_bazel"
default['bazel']['zcomp_sha256']     = '4094dc84add2f23823bc341186adf6b8487fbd5d4164bd52d98891c41511eba4'

default["graalvm-native-image"]         = {}
default["graalvm-native-image"]["path"] = "/opt/graalvm-ce-java17-22.3.0"

default['skaffold']                  = {}
default['kubectl']                   = {}
default['golang']                    = {}
default['minikube']                  = {}
default['nodejs']                    = {}
default['packer']                    = {}
default['kustomize']                 = {}
default['sops']                      = {}
default['shellcheck']                = {}
default['sentry']                    = {}
default['prototool']                 = {}
default['yq']                        = {}
default['src']                       = {}
default['fossa']                     = {}
default['golangci-lint']             = {}
default['helm']                      = {}
default['opm']                       = {}
default['lego']                      = {}
default['codecov']                   = {}
default['gh']                        = {}
default['gcc-musl']                  = {}

if node[:platform] == 'ubuntu'
  default['bazel']['download_path'] =
    "https://github.com/bazelbuild/bazel/releases/download/#{default['bazel']['version']}/bazel-#{default['bazel']['version']}-linux-x86_64"
  default['bazel']['sha256'] =
    'f03d44ecaac3878e3d19489e37caa4ca1dc57427b686a78a85065ea3c27ebe68'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.19.4.linux-amd64.tar.gz'
  default['golang']['sha256'] =
    'c9c08f783325c4cf840a94333159cc937f05f75d36a8b307951d5bd959cf2ab8'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.38.0/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    '3c347c9478880f22ebf95807c13371844769c625cf3ea9c987cd85859067503c'

  default['kubectl']['download_path'] =
    'https://storage.googleapis.com/kubernetes-release/release/v1.21.5/bin/linux/amd64/kubectl'
  default['kubectl']['sha256']        = '060ede75550c63bdc84e14fcc4c8ab3017f7ffc032fc4cac3bf20d274fab1be4'

  default['minikube']['download_path'] =
    'https://github.com/kubernetes/minikube/releases/download/v1.24.0/minikube-linux-amd64'
  default['minikube']['sha256']        =
    '3bc218476cf205acf11b078d45210a4882e136d24a3cbb7d8d645408e423b8fe'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v16.13.2/node-v16.13.2-linux-x64.tar.xz'
  default['nodejs']['sha256']        = '7f5e9a42d6e86147867d35643c7b1680c27ccd45db85666fc52798ead5e74421'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.7.8/packer_1.7.8_linux_amd64.zip'
  default['packer']['sha256']        = '8a94b84542d21b8785847f4cccc8a6da4c7be5e16d4b1a2d0a5f7ec5532faec0'

  default['kustomize']['download_path'] =
    'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.2.0/kustomize_3.2.0_linux_amd64'
  default['kustomize']['sha256']        =
    '7db89e32575d81393d5d84f0dc6cbe444457e61ce71af06c6e6b7b6718299c22'

  default['sops']['download_path'] =
    'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.linux'
  default['sops']['sha256']        =
    '6eacdd01b68fd140eb71bbca233bea897cccb75dbf9e00a02e648b2f9a8a6939'

  default['shellcheck']['download_path'] =
    'https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.linux.x86_64.tar.xz'
  default['shellcheck']['sha256']        =
    '39c501aaca6aae3f3c7fc125b3c3af779ddbe4e67e4ebdc44c2ae5cba76c847f'

  default['sentry']['download_path'] =
    'https://github.com/getsentry/sentry-cli/releases/download/1.52.0/sentry-cli-Linux-x86_64'
  default['sentry']['sha256']        =
    'd6aeb45efbcdd3ec780f714b5082046ea1db31ff60ed0fc39916bbc8b6d708be'

  default['prototool']['download_path'] =
    'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Linux-x86_64'
  default['prototool']['sha256']        =
    '2247ff34ad31fa7d9433b3310879190d1ab63b2ddbd58257d24c267f53ef64e6'

  default['yq']['download_path'] =
    'https://github.com/mikefarah/yq/releases/download/v4.13.4/yq_linux_amd64'
  default['yq']['sha256']        =
    '11092943c548232bc670504303807e5f4b68adc9690fae74069c1c7f5dff0f3f'

  default['src']['download_path'] =
    'https://github.com/sourcegraph/src-cli/releases/download/3.22.3/src_linux_amd64'
  default['src']['sha256']        =
    '4bf9cf9756bc2b117eccbd4bb259a8187066833bf9babf9086577907563bbaec'

  default['golangci-lint']['download_path'] =
    'https://github.com/golangci/golangci-lint/releases/download/v1.48.0/golangci-lint-1.48.0-linux-amd64.tar.gz'
  default['golangci-lint']['sha256']        =
    '127c5c9d47cf3a3cf4128815dea1d9623d57a83a22005e91b986b0cbceb09233'

  default['helm']['download_path'] = 'https://get.helm.sh/helm-v3.5.2-linux-amd64.tar.gz'
  default['helm']['sha256']        = '01b317c506f8b6ad60b11b1dc3f093276bb703281cb1ae01132752253ec706a2'

  default['opm']['download_path'] =
    'https://github.com/operator-framework/operator-registry/releases/download/v1.17.3/linux-amd64-opm'
  default['opm']['sha256']        =
    '771b72d802ac58b740ac493caf79256b28686907d3578f3b1e1e77b570bda156'

  default['faq']['download_path'] =
    'https://github.com/jzelinskie/faq/releases/download/0.0.7/faq-linux-amd64'
  default['faq']['sha256']        =
    '6c9234d0b2b024bf0e7c845fc092339b51b94e5addeee9612a7219cfd2a7b731'

  default['fossa']['download_path'] =
    'https://github.com/fossas/fossa-cli/releases/download/v1.1.10/fossa-cli_1.1.10_linux_amd64.tar.gz'
  default['fossa']['sha256']        =
    'a263aabf09308614a39d8486df722f3b03ab5b0f5060b655be1fd9def8e5619f'

  default['lego']['download_path'] =
    'https://github.com/go-acme/lego/releases/download/v4.5.3/lego_v4.5.3_linux_amd64.tar.gz'
  default['lego']['sha256']        =
    'd6a6dbf82ae9a1a7f9fbc8d85c224617a17337afa4284aaca6b0556a7347609d'

  default['codecov']['download_path'] =
    'https://uploader.codecov.io/v0.2.3/linux/codecov'
  default['codecov']['sha256'] =
    '648b599397548e4bb92429eec6391374c2cbb0edb835e3b3f03d4281c011f401'

  default['gh']['download_path'] =
    'https://github.com/cli/cli/releases/download/v2.12.1/gh_2.12.1_linux_amd64.tar.gz'
  default['gh']['sha256']        =
    '359ff9d759b67e174214098144a530a8afc4b0c9d738cd07c83ac84390cdc988'

  default['gcc-musl']['deb']        =
    'https://storage.googleapis.com/pixie-dev-public/gcc-musl-libs-11.2.0.deb'
  default['gcc-musl']['deb_sha256'] =
    'ba52df92bce02f3c2bc53604466e0fac8844f941ae6d2d44061e48403f5752fb'
  default['gcc-musl']['version']    = "11.2.0"

  default['graalvm-native-image']['deb'] =
    'https://storage.googleapis.com/pixie-dev-public/graalvm-native-image-22.3.0.deb'
  default['graalvm-native-image']['deb_sha256'] =
    '19c8e25511fd9a364ff301e34771abb262e34ab0df4ffef2df36497af4abe1b7'
  default['graalvm-native-image']['version'] = "22.3.0"
elsif node[:platform] == 'mac_os_x' || node[:platform] == 'macos'
  default['bazel']['download_path'] =
    "https://github.com/bazelbuild/bazel/releases/download/#{default['bazel']['version']}/bazel-#{default['bazel']['version']}-darwin-x86_64"
  default['bazel']['sha256'] =
    '8e543c5c9f1c8c91df945cd2fb4c3b43587929a43044a0ed87d13da0d19f96e8'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.19.4.darwin-amd64.tar.gz'
  default['golang']['sha256'] =
    '44894862d996eec96ef2a39878e4e1fce4d05423fc18bdc1cbba745ebfa41253'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.38.0/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = '872897d78a17812913cd6e930c5d1c94f7c862381db820815c4bffc637c28b88'

  default['kubectl']['download_path'] =
    'https://storage.googleapis.com/kubernetes-release/release/v1.21.5/bin/darwin/amd64/kubectl'
  default['kubectl']['sha256']        = '54be977d44dc7f1960d3605f69756e891c216b303342750e035748f6a2eab9d7'

  default['minikube']['download_path'] =
    'https://github.com/kubernetes/minikube/releases/download/v1.24.0/minikube-darwin-amd64'
  default['minikube']['sha256']        = '55f14e4f411370da18d7b9432ae4edd128f4f047bbc87a278e08ba256ff6f669'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v16.13.2/node-v16.13.2-darwin-x64.tar.gz'
  default['nodejs']['sha256']        = '900a952bb77533d349e738ff8a5179a4344802af694615f36320a888b49b07e6'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.7.8/packer_1.7.8_darwin_amd64.zip'
  default['packer']['sha256']        = 'f8abe5d8660be2e6bea04bbb165ede4026e66f2f48ae5f076f9ea858699357ae'

  default['kustomize']['download_path'] =
    'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.2.0/kustomize_3.2.0_darwin_amd64'
  default['kustomize']['sha256']        =
    'c7991a79470a52a95f1fac33f588b76f64e597ac64b54106e452f3a8f642c62e'

  default['sops']['download_path'] =
    'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.darwin'
  default['sops']['sha256']        =
    '09bb5920ae609bdf041b74843e2d8211a7059847b21729fadfbd3c3e33e67d26'

  default['shellcheck']['download_path'] =
    'https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.darwin.x86_64.tar.xz'
  default['shellcheck']['sha256']        =
    'c4edf1f04e53a35c39a7ef83598f2c50d36772e4cc942fb08a1114f9d48e5380'

  default['sentry']['download_path'] =
    'https://github.com/getsentry/sentry-cli/releases/download/1.52.0/sentry-cli-Darwin-x86_64'
  default['sentry']['sha256']        =
    '97c9bafbcf87bd7dea4f1069fe18f8e8265de6f7eab20c62ca9299e0fa8c2af6'

  default['prototool']['download_path'] =
    'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Darwin-x86_64'
  default['prototool']['sha256']        =
    '5ca2a19f1cb04bc5059bb07e14d565231246e623f7523fafe9389b463addf645'

  default['yq']['download_path'] =
    'https://github.com/mikefarah/yq/releases/download/v4.13.4/yq_darwin_amd64'
  default['yq']['sha256']        =
    '17ab1aa6589f5be6398c60acc875426f4f64faeaba6ee581f700b0a9f47da19e'

  default['src']['download_path'] =
    'https://github.com/sourcegraph/src-cli/releases/download/3.22.3/src_darwin_amd64'
  default['src']['sha256']        =
    '6d3ff2d9222b90248ca8311f6ffbcd050a3a7484fd94b71f49ecf2866a38b315'

  default['golangci-lint']['download_path'] =
    'https://github.com/golangci/golangci-lint/releases/download/v1.48.0/golangci-lint-1.48.0-darwin-amd64.tar.gz'
  default['golangci-lint']['sha256']        =
    'ec2e1c3bb3d34268cd57baba6b631127beb185bbe8cfde8ac40ba9b4c8615784'

  default['helm']['download_path'] = 'https://get.helm.sh/helm-v3.5.2-darwin-amd64.tar.gz'
  default['helm']['sha256']        = '68040e9a2f147a92c2f66ce009069826df11f9d1e1c6b78c7457066080ad3229'

  default['opm']['download_path'] =
    'https://github.com/operator-framework/operator-registry/releases/download/v1.17.3/darwin-amd64-opm'
  default['opm']['sha256']        =
    'bb812b97fb3c65f634ecc26910b2dfc0f2e668b86272702200937937df83ad5a'

  default['faq']['download_path'] =
    'https://github.com/jzelinskie/faq/releases/download/0.0.7/faq-darwin-amd64'
  default['faq']['sha256']        =
    '869f4d8acaa1feb11ce76b2204c5476b8a04d9451216adde6b18e2ef2f978794'

  default['fossa']['download_path'] =
    'https://github.com/fossas/fossa-cli/releases/download/v1.1.10/fossa-cli_1.1.10_darwin_amd64.tar.gz'
  default['fossa']['sha256']        =
    '39f23d382c63381ec98e0b22cbf60c2007bdb699b034bfd37692a062ba254a8d'

  default['lego']['download_path'] =
    'https://github.com/go-acme/lego/releases/download/v4.5.3/lego_v4.5.3_darwin_amd64.tar.gz'
  default['lego']['sha256']        =
    'eaf2792d9731c911da671a6145eebd5ba136c20446adb542e7b1463ffe868388'

  default['codecov']['download_path'] =
    'https://uploader.codecov.io/v0.2.3/macos/codecov'
  default['codecov']['sha256'] =
    '8d3709d957c7115610e764621569728be102d213fee15bc1d1aa9d465eb2c258'

  default['gh']['download_path'] =
    'https://github.com/cli/cli/releases/download/v2.12.1/gh_2.12.1_macOS_amd64.tar.gz'
  default['gh']['sha256']        =
    '448d617c11b964cff135bab43f73b321386c09fc5cdd998a17cbfc422f54239e'
end
