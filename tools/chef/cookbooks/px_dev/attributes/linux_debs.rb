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

default['clang-linters']['version']    = '15.0-pl9'
default['clang-linters']['deb']        =
  "https://storage.googleapis.com/pixie-dev-public/clang/#{default['clang-linters']['version']}/clang-linters-#{default['clang-linters']['version']}.deb"
default['clang-linters']['deb_sha256'] =
  '8a954e9a7e89cf97f91a3bea0119422f6c2f8044380bd204e56b96a098637a2c'
