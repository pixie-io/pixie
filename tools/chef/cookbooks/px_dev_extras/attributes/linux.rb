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

default['docker-buildx']['download_path'] =
  'https://github.com/docker/buildx/releases/download/v0.10.4/buildx-v0.10.4.linux-amd64'
default['docker-buildx']['sha256'] =
  'dbe68cdc537d0150fc83e3f30974cd0ca11c179dafbf27f32d6f063be26e869b'

default['faq']['download_path'] =
  'https://github.com/jzelinskie/faq/releases/download/0.0.7/faq-linux-amd64'
default['faq']['sha256'] =
  '6c9234d0b2b024bf0e7c845fc092339b51b94e5addeee9612a7219cfd2a7b731'

default['gh']['download_path'] =
  'https://github.com/cli/cli/releases/download/v2.76.1/gh_2.76.1_linux_amd64.tar.gz'
default['gh']['sha256'] =
  '18367ca38b4462889ae38fba6a18c53a4c2818b6af309bbe53d0810bb06036e9'

default['helm']['download_path'] =
  'https://get.helm.sh/helm-v3.11.3-linux-amd64.tar.gz'
default['helm']['sha256'] =
  'ca2d5d40d4cdfb9a3a6205dd803b5bc8def00bd2f13e5526c127e9b667974a89'

default['kubectl']['download_path'] =
  'https://dl.k8s.io/release/v1.33.3/bin/linux/amd64/kubectl'
default['kubectl']['sha256'] =
  '2fcf65c64f352742dc253a25a7c95617c2aba79843d1b74e585c69fe4884afb0'

default['kustomize']['download_path'] =
  'https://github.com/kubernetes-sigs/kustomize/releases/download/kustomize%2Fv5.0.3/kustomize_v5.0.3_linux_amd64.tar.gz'
default['kustomize']['sha256'] =
  'c627b1575c3fecbc7ad1c181c23a7adcacf19732dab627eb57e89a7bc4c1e929'

default['lego']['download_path'] =
  'https://github.com/go-acme/lego/releases/download/v4.5.3/lego_v4.5.3_linux_amd64.tar.gz'
default['lego']['sha256'] =
  'd6a6dbf82ae9a1a7f9fbc8d85c224617a17337afa4284aaca6b0556a7347609d'

default['minikube']['download_path'] =
  'https://github.com/kubernetes/minikube/releases/download/v1.36.0/minikube-linux-amd64'
default['minikube']['sha256'] =
  'cddeab5ab86ab98e4900afac9d62384dae0941498dfbe712ae0c8868250bc3d7'

default['opm']['download_path'] =
  'https://github.com/operator-framework/operator-registry/releases/download/v1.26.4/linux-amd64-opm'
default['opm']['sha256'] =
  'cf94e9dbd58c338e1eed03ca50af847d24724b99b40980812abbe540e8c7ff8e'

default['packer']['download_path'] =
  'https://releases.hashicorp.com/packer/1.7.8/packer_1.7.8_linux_amd64.zip'
default['packer']['sha256'] =
  '8a94b84542d21b8785847f4cccc8a6da4c7be5e16d4b1a2d0a5f7ec5532faec0'

default['skaffold']['download_path'] =
  'https://storage.googleapis.com/skaffold/releases/v2.16.1/skaffold-linux-amd64'
default['skaffold']['sha256'] =
  '1cbeea85aa14ba603dbc2bbdfa7bfde5644d7988beed0fdc0fd1c67298d4cf67'

default['sops']['download_path'] =
  'https://github.com/mozilla/sops/releases/download/v3.10.2/sops-v3.10.2.linux.amd64'
default['sops']['sha256'] =
  '79b0f844237bd4b0446e4dc884dbc1765fc7dedc3968f743d5949c6f2e701739'

default['trivy']['download_path'] =
  'https://github.com/aquasecurity/trivy/releases/download/v0.64.1/trivy_0.64.1_Linux-64bit.tar.gz'
default['trivy']['sha256'] =
  '1a09d86667b3885a8783d1877c9abc8061b2b4e9b403941b22cbd82f10d275a8'

default['yq']['download_path'] =
  'https://github.com/mikefarah/yq/releases/download/v4.30.8/yq_linux_amd64'
default['yq']['sha256'] =
  '6c911103e0dcc54e2ba07e767d2d62bcfc77452b39ebaee45b1c46f062f4fd26'
