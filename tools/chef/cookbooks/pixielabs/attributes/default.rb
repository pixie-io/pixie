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
  'https://storage.googleapis.com/pixie-dev-public/clang-13.0-pl1.deb'
default['clang']['deb_sha256'] =
  'f913dc8d3fa897b0e8a4b4a158ba85de364000c7a36aee71202143dd5e76d67c'
default['clang']['version']    = "13.0-pl1"

default['gperftools']               = {}
default['gperftools']['deb']        =
  'https://storage.googleapis.com/pixie-dev-public/gperftools-pixie-2.7-pl2.deb'
default['gperftools']['deb_sha256'] =
  '10f00d6ddde920c74c80eb966362fa234c1d97349ba186b51d05bb98e3fff72e'
default['gperftools']['version']    = "2.7-pl2"

default['gsutil']                  = {}
default['gsutil']['download_path'] = 'https://storage.googleapis.com/pub/gsutil_4.54.tar.gz'
default['gsutil']['sha256']        = 'a6698479af8dc26e2ed809102e9e5d813f475bca44ce7007ed4e25ee79a3289c'

default['antlr'] = {}
default['antlr']['download_path'] = 'https://www.antlr.org/download/antlr-4.9-complete.jar'
default['antlr']['sha256'] = 'bd11b2464bc8aee5f51b119dff617101b77fa729540ee7f08241a6a672e6bc81'

default['skaffold']                  = {}
default['kubectl']                   = {}
default['bazel']                     = {}
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
default['lsif-go']                   = {}
default['fossa']                     = {}
default['golangci-lint']             = {}
default['helm']                      = {}
default['opm']                       = {}
default['lego']                      = {}


if node[:platform] == 'ubuntu'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/4.2.2/bazel-4.2.2-linux-x86_64'
  default['bazel']['sha256'] =
    '11dea6c7cfd866ed520af19a6bb1d952f3e9f4ee60ffe84e63c0825d95cb5859'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.16.linux-amd64.tar.gz'
  default['golang']['sha256'] =
    '013a489ebb3e24ef3d915abe5b94c3286c070dfe0818d5bca8108f1d6e8440d2'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.12.1/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    'e96db5103448663d349072c515ddae33bdf05727689a9a3460f3f36a41a94b8e'

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

  default['lsif-go']['download_path'] =
    'https://github.com/sourcegraph/lsif-go/releases/download/v1.3.0/src_linux_amd64'
  default['lsif-go']['sha256']        =
    '82eb998370b05d2d9c05664f7270599ddcef676c1d211274a5e04ffddf6ac024'

  default['golangci-lint']['download_path'] =
    'https://github.com/golangci/golangci-lint/releases/download/v1.39.0/golangci-lint-1.39.0-linux-amd64.tar.gz'
  default['golangci-lint']['sha256']        =
    '3a73aa7468087caa62673c8adea99b4e4dff846dc72707222db85f8679b40cbf'

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
elsif node[:platform] == 'mac_os_x'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/4.2.2/bazel-4.2.2-darwin-x86_64'
  default['bazel']['sha256'] =
    '288660a310193c492a38a0480c42c74789564c09511e6adc045b5b4b4f117f7d'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.16.darwin-amd64.tar.gz'
  default['golang']['sha256'] =
    '6000a9522975d116bf76044967d7e69e04e982e9625330d9a539a8b45395f9a8'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.12.1/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = '6b2bd0ae47dda96d64661136222622d97aaa9bd020b67f77fb744f944cd47ae5'

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

  default['lsif-go']['download_path'] =
    'https://github.com/sourcegraph/lsif-go/releases/download/v1.3.0/src_darwin_amd64'
  default['lsif-go']['sha256']        =
    'a8ad2b7169763978a63605ac854473998729ba7c497dd39bed01b57a294bd32a'

  default['golangci-lint']['download_path'] =
    'https://github.com/golangci/golangci-lint/releases/download/v1.39.0/golangci-lint-1.39.0-darwin-amd64.tar.gz'
  default['golangci-lint']['sha256']        =
    '7e9a47ab540aa3e8472fbf8120d28bed3b9d9cf625b955818e8bc69628d7187c'

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
end
