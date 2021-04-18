#!/usr/bin/env python3

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

# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import glob
import os
import argparse

py_header_txt = """# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""

go_header_txt = """/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"""


def add_header(fname: str, header: str) -> None:
    dummy_file = fname + '.bak'
    with open(fname, 'r') as f, open(dummy_file, 'w') as fdummy:
        fdummy.write(header)
        for i, line in enumerate(f):
            if i == 0 and line[:2] == "#!":
                # Clear contents, write shebang line, then write header.
                fdummy.truncate(0)
                fdummy.write(line)
                fdummy.write(header)
                continue

            fdummy.write(line)
    os.remove(fname)
    os.rename(dummy_file, fname)


parser = argparse.ArgumentParser(
    description='Preprend licenses to all python and go files in directory')
parser.add_argument('directory', type=str,
                    help='the directory to use.')
args = parser.parse_args()

path = args.directory

py_files = glob.glob(f'{path}/**/*.py', recursive=True)
go_files = glob.glob(f'{path}/**/*.go', recursive=True)

for py_file in py_files:
    add_header(py_file, py_header_txt)

for go_file in go_files:
    add_header(go_file, go_header_txt)
