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

default['clang']['version']    = '15.0-pl9'
default['clang']['deb']        =
  "https://storage.googleapis.com/pixie-dev-public/clang/#{default['clang']['version']}/clang-#{default['clang']['version']}.deb"
default['clang']['deb_sha256'] =
  'a7c6aa046cb3a75fae2f61e1ed43abbcce6514dd91f933bdc20f6633113994ed'

default['gperftools']['version']    = '2.10-pl1'
default['gperftools']['deb']        =
  "https://github.com/pixie-io/dev-artifacts/releases/download/gperftools%2F#{default['gperftools']['version']}/gperftools-pixie-#{default['gperftools']['version']}.deb"
default['gperftools']['deb_sha256'] =
  '0920a93a8a8716b714b9b316c8d7e8f2ecc242a85147f7bec5e1543d88c203dc'
