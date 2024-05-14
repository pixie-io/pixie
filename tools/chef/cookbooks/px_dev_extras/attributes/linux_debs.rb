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

default['clang']['version']    = '15.0-pl12'
default['clang']['deb']        =
  "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F#{default['clang']['version']}/clang-#{default['clang']['version']}.deb"
default['clang']['deb_sha256'] =
  '3aef15345f70d00feaf0fada0eb76ac169e190a08576d3c375bef1b04400e552'

# The pixie built clang deb originates from bionic to keep glibc compatibility on older systems (see tools/docker/clang_deb_image/Dockerfile).
# This causes the clang binary installed above to dynamically linking libtinfo5. Starting with Ubuntu 24.04, libtinfo6 is provided upstream
# so we need to install this to have a functional /opt/px_dev clang binary.
default['libtinfo5']['version']    = '6.3-2ubuntu0.1'
default['libtinfo5']['deb']        =
  "https://github.com/pixie-io/dev-artifacts/releases/download/libtinfo5%2F#{default['libtinfo5']['version']}/libtinfo5-#{default['libtinfo5']['version']}.deb"
default['libtinfo5']['deb_sha256'] =
  'ab89265d8dd18bda6a29d7c796367d6d9f22a39a8fa83589577321e7caf3857b'

default['gperftools']['version']    = '2.10-pl1'
default['gperftools']['deb']        =
  "https://github.com/pixie-io/dev-artifacts/releases/download/gperftools%2F#{default['gperftools']['version']}/gperftools-pixie-#{default['gperftools']['version']}.deb"
default['gperftools']['deb_sha256'] =
  '0920a93a8a8716b714b9b316c8d7e8f2ecc242a85147f7bec5e1543d88c203dc'
