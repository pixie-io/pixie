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
  'https://storage.googleapis.com/pixie-dev-public/clang-11.1-pl1.deb'
default['clang']['deb_sha256'] =
  '545fb355f6a189cafc32aa49587ffaebbc3fa61253e0d8a5525b50941c8b1ebf'
default['clang']['version']    = "11.1-pl1"

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
default['golangci-lint']             = {}
default['helm']                      = {}
default['opm']                       = {}


if node[:platform] == 'ubuntu'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/4.0.0/bazel-4.0.0-linux-x86_64'
  default['bazel']['sha256'] =
    '7bee349a626281fc8b8d04a7a0b0358492712377400ab12533aeb39c2eb2b901'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.16.linux-amd64.tar.gz'
  default['golang']['sha256'] =
    '013a489ebb3e24ef3d915abe5b94c3286c070dfe0818d5bca8108f1d6e8440d2'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.12.1/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    'e96db5103448663d349072c515ddae33bdf05727689a9a3460f3f36a41a94b8e'

  default['kubectl']['download_path'] =
    'https://storage.googleapis.com/kubernetes-release/release/v1.14.6/bin/linux/amd64/kubectl'
  default['kubectl']['sha256']        = '5f8e8d8de929f64b8f779d0428854285e1a1c53a02cc2ad6b1ce5d32eefad25c'

  default['minikube']['download_path'] =
    'https://github.com/kubernetes/minikube/releases/download/v1.9.2/minikube-linux-amd64'
  default['minikube']['sha256']        =
    '3121f933bf8d608befb24628a045ce536658738c14618504ba46c92e656ea6b5'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v16.1.0/node-v16.1.0-linux-x64.tar.xz'
  default['nodejs']['sha256']        = '94d14ed1871a69e3dedd3a54d8c547c978b49566892616a227bf8be2f171a8a8'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.3.2/packer_1.3.2_linux_amd64.zip'
  default['packer']['sha256']        = '5e51808299135fee7a2e664b09f401b5712b5ef18bd4bad5bc50f4dcd8b149a1'

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
    'https://github.com/mikefarah/yq/releases/download/3.2.1/yq_linux_amd64'
  default['yq']['sha256']        =
    '11a830ffb72aad0eaa7640ef69637068f36469be4f68a93da822fbe454e998f8'

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
elsif node[:platform] == 'mac_os_x'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/4.0.0/bazel-4.0.0-darwin-x86_64'
  default['bazel']['sha256'] =
    '349f3c9dd24191369c1073c57cc1386fc3c2d4ad7d44135c3d873c9dc67fae1f'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.16.darwin-amd64.tar.gz'
  default['golang']['sha256'] =
    '6000a9522975d116bf76044967d7e69e04e982e9625330d9a539a8b45395f9a8'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.12.1/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = '6b2bd0ae47dda96d64661136222622d97aaa9bd020b67f77fb744f944cd47ae5'

  default['kubectl']['download_path'] =
    'https://storage.googleapis.com/kubernetes-release/release/v1.14.6/bin/darwin/amd64/kubectl'
  default['kubectl']['sha256']        = 'de42dd22f67c135b749c75f389c70084c3fe840e3d89a03804edd255ac6ee829'

  default['minikube']['download_path'] =
    'https://github.com/kubernetes/minikube/releases/download/v1.9.2/minikube-darwin-amd64'
  default['minikube']['sha256']        = 'f27016246850b3145e1509e98f7ed060fd9575ac4d455c7bdc15277734372e85'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v16.1.0/node-v16.1.0-darwin-x64.tar.gz'
  default['nodejs']['sha256']        = '22525ecc3b91f4d9a5d44dffe061cdb23f1a3e4a5555552e7940987883a93547'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.3.2/packer_1.3.2_darwin_amd64.zip'
  default['packer']['sha256']        = '1c2433239d801b017def8e66bbff4be3e7700b70248261b0abff2cd9c980bf5b'

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
    'https://github.com/mikefarah/yq/releases/download/3.2.1/yq_darwin_amd64'
  default['yq']['sha256']        =
    '116f74a384d0b4fa31a58dd01cfcdeffa6fcd21c066de223cbb0ebc042a8bc28'

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
  
end
