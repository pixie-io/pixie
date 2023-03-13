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

def format(target):
    build_opts = build_options(target)

    # We only want to get targets that are in the target configuration. So we ignore exec and host targets.
    if build_opts["//command_line_option:is exec configuration"] or build_opts["//command_line_option:is host configuration"]:
        return None

    # Ignore targets that are incompatible with the target configuration.
    if providers(target) and "IncompatiblePlatformProvider" in providers(target):
        return None
    return target.label
