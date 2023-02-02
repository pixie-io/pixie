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

default['faq']['download_path'] =
  'https://github.com/jzelinskie/faq/releases/download/0.0.7/faq-darwin-amd64'
default['faq']['sha256'] =
  '869f4d8acaa1feb11ce76b2204c5476b8a04d9451216adde6b18e2ef2f978794'

default['gh']['download_path'] =
  'https://github.com/cli/cli/releases/download/v2.12.1/gh_2.12.1_macOS_amd64.tar.gz'
default['gh']['sha256'] =
  '448d617c11b964cff135bab43f73b321386c09fc5cdd998a17cbfc422f54239e'

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

default['opm']['download_path'] =
  'https://github.com/operator-framework/operator-registry/releases/download/v1.17.3/darwin-amd64-opm'
default['opm']['sha256'] =
  'bb812b97fb3c65f634ecc26910b2dfc0f2e668b86272702200937937df83ad5a'

default['packer']['download_path'] =
  'https://releases.hashicorp.com/packer/1.7.8/packer_1.7.8_darwin_amd64.zip'
default['packer']['sha256'] =
  '4435f3ab48eb0759f52cc9773182746f004ab0cc697efa23731c77b109d25b64'

default['skaffold']['download_path'] =
  'https://storage.googleapis.com/skaffold/releases/v2.0.4/skaffold-darwin-amd64'
default['skaffold']['sha256'] =
  'd0956712db4d2dd8084ffe297bf645ec92506b87db5f61c0f5e24c7fd99bf0a3'

default['sops']['download_path'] =
  'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.darwin'
default['sops']['sha256'] =
  '09bb5920ae609bdf041b74843e2d8211a7059847b21729fadfbd3c3e33e67d26'

default['yq']['download_path'] =
  'https://github.com/mikefarah/yq/releases/download/v4.30.8/yq_darwin_amd64'
default['yq']['sha256'] =
  '123a992cada25421db5c068895006047d3dcdb61987c00e93a1127e6af61b93a'
