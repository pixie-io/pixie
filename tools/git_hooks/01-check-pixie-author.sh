#!/bin/bash -e

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

pattern='pixielabs\.ai$'

while read -r _ newsha _; do
	if [[ "${newsha}" == "0000000000000000000000000000000000000000" ]]; then
		continue
	fi

	author=$(git log -1 --pretty=format:"%ae" "${newsha}")
	committer=$(git log -1 --pretty=format:"%ce" "${newsha}")
	if [[ ! "${author}" =~ ${pattern} ]]; then
		if [[ "${committer}" =~ ^(zasgar|vihang|michellenguyen)@pixielabs\.ai$ ]]; then
            # Allow a small set of committers to use a non pixielabs email for
            # the author field. This is for external contributions.
			echo "WARNING: Non pixielabs author"
		else
			echo "======================================================================="
			echo "Please set your gitconfig to use your pixielabs.ai email"
			echo "Found author email: ${author}"
			echo "======================================================================="
			exit 1
		fi
	fi
done
