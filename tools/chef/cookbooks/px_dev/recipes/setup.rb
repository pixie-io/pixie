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

dirs = ['/opt/px_dev', '/opt/px_dev/bin', '/opt/px_dev/gopath', '/opt/px_dev/tools']

dirs.each do |path|
  directory path do
    owner node['owner']
    group node['group']
    mode '0755'
    action :create
  end
end

include_recipe 'px_dev::golang'
include_recipe 'px_dev::nodejs'
include_recipe 'px_dev::php'
include_recipe 'px_dev::python'

include_recipe 'px_dev::arcanist'
