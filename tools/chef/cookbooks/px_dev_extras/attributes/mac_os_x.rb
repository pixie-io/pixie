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
  'https://github.com/cli/cli/releases/download/v2.76.1/gh_2.76.1_macOS_amd64.tar.gz'
default['gh']['sha256'] =
  '0019dfc4b32d63c1392aa264aed2253c1e0c2fb09216f8e2cc269bbfb8bb49b5'

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
  'https://github.com/kubernetes/minikube/releases/download/v1.36.0/minikube-darwin-amd64'
default['minikube']['sha256'] =
  'a7e3da0db4041b2f845ca37af592424a9cbe34087ac922220b1e3abc4e1976ea'

default['opm']['download_path'] =
  'https://github.com/operator-framework/operator-registry/releases/download/v1.26.4/darwin-amd64-opm'
default['opm']['sha256'] =
  '9c5d21fd4200373e8ee8c0df47ac4c06c1762e807f836d5382d67e2f47d6fa1b'

default['packer']['download_path'] =
  'https://releases.hashicorp.com/packer/1.7.8/packer_1.7.8_darwin_amd64.zip'
default['packer']['sha256'] =
  '8666031111138e2e79ff7d1e42888b23b793b856bc2d9c9dccbb1e2d2cccb5cf'

default['skaffold']['download_path'] =
  'https://storage.googleapis.com/skaffold/releases/v2.16.1/skaffold-darwin-amd64'
default['skaffold']['sha256'] =
  'ed4c6cd0c82f48908db6bc1210b2f609bb5672b340ad1fbaa092ed2c6acedeb5'

default['sops']['download_path'] =
  'https://github.com/mozilla/sops/releases/download/v3.10.2/sops-v3.10.2.darwin.amd64'
default['sops']['sha256'] =
  'dece9b0131af5ced0f8c278a53c0cf06a4f0d1d70a177c0979f6d111654397ce'

default['trivy']['download_path'] =
  'https://github.com/aquasecurity/trivy/releases/download/v0.64.1/trivy_0.64.1_macOS-64bit.tar.gz'
default['trivy']['sha256'] =
  '107a874b41c1f0a48849f859b756f500d8be06f2d2b8956a046a97ae38088bf6'

default['yq']['download_path'] =
  'https://github.com/mikefarah/yq/releases/download/v4.30.8/yq_darwin_amd64'
default['yq']['sha256'] =
  '123a992cada25421db5c068895006047d3dcdb61987c00e93a1127e6af61b93a'
