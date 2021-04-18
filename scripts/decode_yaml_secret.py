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

import yaml
import sys
import base64

# Takes a k8s secret yaml from stdout, and base64 decodes the value at each field,
# writing the result back to stdout. Used for service_tls_certs.


def str_presenter(dumper, data):
    if len(data.splitlines()) > 1:  # check for multiline string
        return dumper.represent_scalar('tag:yaml.org,2002:str', data, style='|')
    return dumper.represent_scalar('tag:yaml.org,2002:str', data)


def main():
    lines = ""
    for line in sys.stdin:
        lines += line

    data = yaml.load(lines)
    data['stringData'] = {}
    for k, v in data['data'].items():
        data['stringData'][k] = base64.b64decode(v)

    del data['data']

    metadata_keys = list(data['metadata'])
    for mk in metadata_keys:
        if mk != "name":
            del data['metadata'][mk]

    yaml.add_representer(str, str_presenter)
    yaml.dump(data, sys.stdout)


if __name__ == '__main__':
    main()
