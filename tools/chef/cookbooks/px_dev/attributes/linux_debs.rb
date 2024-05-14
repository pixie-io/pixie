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

default['clang-linters']['version']    = '15.0-pl12'
default['clang-linters']['deb']        =
  "https://github.com/pixie-io/dev-artifacts/releases/download/clang%2F#{default['clang-linters']['version']}/clang-linters-#{default['clang-linters']['version']}.deb"
default['clang-linters']['deb_sha256'] =
  'f264b9aa1afab52d732282a0e2177d8a372cefc71d791fd45e6e2e4df4e8ac43'
