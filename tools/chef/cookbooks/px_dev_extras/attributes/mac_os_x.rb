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

default['docker-buildx']['download_path'] =
  'https://github.com/docker/buildx/releases/download/v0.10.4/buildx-v0.10.4.darwin-amd64'
default['docker-buildx']['sha256'] =
  '63aadf0095a583963c9613b3bc6e5782c8c56ed881ca9aa65f41896f4267a9ee'

default['faq']['download_path'] =
  'https://github.com/jzelinskie/faq/releases/download/0.0.7/faq-darwin-amd64'
default['faq']['sha256'] =
  '869f4d8acaa1feb11ce76b2204c5476b8a04d9451216adde6b18e2ef2f978794'

default['gh']['download_path'] =
  'https://github.com/cli/cli/releases/download/v2.26.1/gh_2.26.1_macOS_amd64.tar.gz'
default['gh']['sha256'] =
  'ef398ece1f31d033df6374458f7a87500ccdbdc9964170db04b6a5f707632417'

default['helm']['download_path'] =
  'https://get.helm.sh/helm-v3.11.3-darwin-amd64.tar.gz'
default['helm']['sha256'] =
  '9d029df37664b50e427442a600e4e065fa75fd74dac996c831ac68359654b2c4'

default['kubectl']['download_path'] =
  'https://storage.googleapis.com/kubernetes-release/release/v1.26.0/bin/darwin/amd64/kubectl'
default['kubectl']['sha256'] =
  'be9dc0782a7b257d9cfd66b76f91081e80f57742f61e12cd29068b213ee48abc'

default['kustomize']['download_path'] =
  'https://github.com/kubernetes-sigs/kustomize/releases/download/kustomize%2Fv5.0.3/kustomize_v5.0.3_darwin_amd64.tar.gz'
default['kustomize']['sha256'] =
  'a3300ccc81ed8e7df415f3537b49e70d89f985a28c9ade8a885ebf6f1689b4e0'

default['lego']['download_path'] =
  'https://github.com/go-acme/lego/releases/download/v4.5.3/lego_v4.5.3_darwin_amd64.tar.gz'
default['lego']['sha256'] =
  'eaf2792d9731c911da671a6145eebd5ba136c20446adb542e7b1463ffe868388'

default['minikube']['download_path'] =
  'https://github.com/kubernetes/minikube/releases/download/v1.30.1/minikube-darwin-amd64'
default['minikube']['sha256'] =
  'b5938a8772c5565b5d0b795938c367c5190bf65bb51fc55fb2417cb4e1d04ef1'

default['opm']['download_path'] =
  'https://github.com/operator-framework/operator-registry/releases/download/v1.26.4/darwin-amd64-opm'
default['opm']['sha256'] =
  '9c5d21fd4200373e8ee8c0df47ac4c06c1762e807f836d5382d67e2f47d6fa1b'

default['packer']['download_path'] =
  'https://releases.hashicorp.com/packer/1.7.8/packer_1.7.8_darwin_amd64.zip'
default['packer']['sha256'] =
  '8666031111138e2e79ff7d1e42888b23b793b856bc2d9c9dccbb1e2d2cccb5cf'

default['skaffold']['download_path'] =
  'https://storage.googleapis.com/skaffold/releases/v2.0.4/skaffold-darwin-amd64'
default['skaffold']['sha256'] =
  'd0956712db4d2dd8084ffe297bf645ec92506b87db5f61c0f5e24c7fd99bf0a3'

default['sops']['download_path'] =
  'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.darwin'
default['sops']['sha256'] =
  '09bb5920ae609bdf041b74843e2d8211a7059847b21729fadfbd3c3e33e67d26'

default['trivy']['download_path'] =
  'https://github.com/aquasecurity/trivy/releases/download/v0.39.0/trivy_0.39.0_macOS-64bit.tar.gz'
default['trivy']['sha256'] =
  'e0e6831395310452a65cae8dcb142fb743a05b27b0698177e8fad93b24490e19'

default['yq']['download_path'] =
  'https://github.com/mikefarah/yq/releases/download/v4.30.8/yq_darwin_amd64'
default['yq']['sha256'] =
  '123a992cada25421db5c068895006047d3dcdb61987c00e93a1127e6af61b93a'
