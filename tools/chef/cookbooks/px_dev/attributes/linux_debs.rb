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

default['clang']['version']    = '15.0-pl2'
default['clang']['deb']        =
  "https://storage.googleapis.com/pixie-dev-public/clang/#{default['clang']['version']}/clang-#{default['clang']['version']}.deb"
default['clang']['deb_sha256'] =
  '32549362100f3759a0b50b3e1993f58086568696839bbdd88c11602bd3e994e2'

default['clang-linters']['version']    = '15.0-pl7'
default['clang-linters']['deb']        =
  "https://storage.googleapis.com/pixie-dev-public/clang/#{default['clang-linters']['version']}/clang-linters-#{default['clang-linters']['version']}.deb"
default['clang-linters']['deb_sha256'] =
  'b17b42694e1edf408a31d68ba04b33708e6c3fb2fbcbd8749697f3ca10996916'

default['gcc-musl']['version'] = '11.2.0'
default['gcc-musl']['deb'] =
  "https://storage.googleapis.com/pixie-dev-public/gcc-musl-libs-#{default['gcc-musl']['version']}.deb"
default['gcc-musl']['deb_sha256'] =
  'ba52df92bce02f3c2bc53604466e0fac8844f941ae6d2d44061e48403f5752fb'
