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

# The contains additional setup required in our docker image,
# so that we can mount our code and use Golang.
ENV['GOPATH'] = '/pl'
ENV['PATH'] = "#{ENV['GOPATH']}/bin:/usr/lib/go-1.10/bin/:#{ENV['PATH']}"

directory '/pl' do
  mode '0755'
  action :create
end
