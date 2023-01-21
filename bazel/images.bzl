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

def image_replacements(image_map, replace):
    replacements = {}

    for k in image_map.keys():
        image_tag = k
        for old, new in replace.items():
            image_tag = image_tag.replace(old, new)
        replacements[k] = image_tag

    return replacements

def image_map_with_bundle_version(image_map, replace):
    with_version = {}

    for k, v in image_map.items():
        image_tag = k

        for old, new in replace.items():
            image_tag = image_tag.replace(old, new)
        k_with_version = "{0}:{1}".format(image_tag, "$(BUNDLE_VERSION)")
        with_version[k_with_version] = v

    return with_version
