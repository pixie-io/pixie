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

include_recipe 'px_dev::linux'
include_recipe 'px_dev::setup'
include_recipe 'px_dev::linters'

template '/opt/px_dev/pxenv.inc' do
  source 'pxenv.inc.erb'
  owner node['owner']
  group node['group']
  mode '0644'
  action :create
end

common_remote_bin 'codecov'
