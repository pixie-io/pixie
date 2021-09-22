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

import argparse
import logging
import os.path
import re
import sys

apache2_license_header = '''
 Copyright 2018- The Pixie Authors.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 SPDX-License-Identifier: Apache-2.0
'''

mit_license_header = '''
 Copyright 2018- The Pixie Authors.

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 SPDX-License-Identifier: MIT
'''

private_license_header = '''
 Copyright © 2018- Pixie Labs Inc.
 Copyright © 2020- New Relic, Inc.
 All Rights Reserved.

 NOTICE:  All information contained herein is, and remains
 the property of New Relic Inc. and its suppliers,
 if any.  The intellectual and technical concepts contained
 herein are proprietary to Pixie Labs Inc. and its suppliers and
 may be covered by U.S. and Foreign Patents, patents in process,
 and are protected by trade secret or copyright law. Dissemination
 of this information or reproduction of this material is strictly
 forbidden unless prior written permission is obtained from
 New Relic, Inc.

 SPDX-License-Identifier: Proprietary
'''

bpf_gpl_2_license_header = '''
 This code runs using bpf in the Linux kernel.
 Copyright 2018- The Pixie Authors.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 SPDX-License-Identifier: GPL-2.0
'''

license_by_spdx = {
    'Apache-2.0': apache2_license_header,
    'GPL-2.0': bpf_gpl_2_license_header,
    'MIT': mit_license_header,
    'Proprietary': private_license_header,
}


def get_spdx_from_license(path):
    with open(path) as f:
        contents = f.read()
        if 'All Rights Reserved.' in contents:
            return 'Proprietary'
        if 'Apache License, Version 2.0' in contents:
            return 'Apache-2.0'
        if 'Permission is hereby granted, free of charge' in contents:
            return 'MIT'
        if 'GNU GENERAL PUBLIC LICENSE' in contents:
            return 'GPL-2.0'
        raise Exception('Cannot determine license type')


def c_style_license_wrapper(txt: str):
    txt = ' ' + txt.strip()
    txt_with_stars = '\n'.join([' *' + line for line in txt.split('\n')])
    return '/*\n' + txt_with_stars + '\n */'


def sh_style_license_wrapper(txt: str):
    txt = ' ' + txt.strip()
    return '\n'.join(['#' + line for line in txt.split('\n')])


def has_spdx(blob: str):
    return ('\n# SPDX-License-Identifier:' in blob) or \
           ('\n// SPDX-License-Identifier:' in blob) or \
           ('\n * SPDX-License-Identifier:' in blob)


shebang_regex = re.compile(r"^(#!.*)")
# The list of matches, this is an array to avoid an uncertainty with overlapping expression.
# Please keep the list sorted, by language name.
matchers = [
    {
        'name': 'c_style',
        'exprs': [
            re.compile(r"^.*\.(cc|cpp|h|hpp|c|inl)$"),
            re.compile(r"^.*\.(bt)$"),
            re.compile(r"^.*\.(java)$"),
            re.compile(r"^.*\.(js|jsx|ts|tsx)$"),
            re.compile(r"^.*\.(proto)$"),
            re.compile(r"^.*\.(css|scss)$"),
        ],
        'wrapper': c_style_license_wrapper,
    },
    {
        'name': 'go',
        'exprs': [
            re.compile(r"^.*\.go$"),
        ],
        'wrapper': c_style_license_wrapper,
        'skip_lines': [
            re.compile(r"^(// \+build.*)"),
        ]
    },
    {
        'name': 'php',
        'exprs': [
            re.compile(r"^.*\.(php)$"),
        ],
        'wrapper': c_style_license_wrapper,
        'skip_lines': [
            re.compile(r"^(<\?php)"),
        ],
    },
    {
        'name': 'shell_without_shebang',
        'exprs': [
            # Bazel.
            re.compile(r"^.*\.(bazel|bzl)$"),
            re.compile(r"^.*\.(BUILD)$"),
            re.compile(r"BUILD.bazel$$"),
            # Docker file.
            re.compile(r"^Dockerfile$"),
            re.compile(r"^Dockerfile\..*$"),
            # Makefiles.
            re.compile(r"^Makefile$"),
            # Starlark..
            re.compile(r"^.*\.(sky)$"),
            # PxL.
            re.compile(r"^.*\.(pxl)$"),
            # Ruby.
            re.compile(r"^.*\.(rb)$"),
        ],
        'wrapper': sh_style_license_wrapper,
    },
    {
        'name': 'shell_with_shebang',
        'exprs': [
            # Python.
            re.compile(r"^.*\.(py)$"),
            # Shell.
            re.compile(r"^.*\.(sh)$"),
        ],
        'wrapper': sh_style_license_wrapper,
        'skip_lines': [
            shebang_regex,
        ],
    },
]


def is_generated_code(file_path: str):
    return file_path.endswith('.gen.go') or file_path.endswith('.pb.go') or file_path.endswith(
        '.deepcopy.go') or file_path.endswith('/schema.ts')


def is_skipped(file_path: str):
    license_file = file_path in ['LICENSE', 'LICENSE.txt', 'go.mod', 'go.sum']
    return is_generated_code(file_path) or license_file


def parse_args():
    parser = argparse.ArgumentParser(description='Check/Fix license info in file')
    parser.add_argument('-f', required=True, type=str, help='the name of the file to check. ')
    parser.add_argument('-a', required=False, action='store_true', default=False,
                        help='automatically fix the file')
    return parser.parse_args(sys.argv[1:])


def find_matcher(path: str):
    base = os.path.basename(path)
    for m in matchers:
        for e in m['exprs']:
            if e.match(base) is not None:
                return m
    return None


class AddLicenseDiff:
    def __init__(self, filename, start_line, txt):
        self._filename = filename
        self._start_line = start_line
        self._txt = txt

    def phabricator(self):
        # <Filename>:<LineNumber>,<offset>
        s = "{}:{},1\n".format(self._filename, self._start_line)
        s += "<<<<<\n"
        s += "=====\n"
        s += self._txt + '\n'
        s += ">>>>>"
        return s

    def fix(self, filepath):
        file_lines = None
        with open(filepath, 'r') as f:
            file_lines = f.readlines()

        with open(filepath, 'w') as f:
            for idx, l in enumerate(file_lines):
                # The line is 1 indexed.
                if (idx + 1) == self._start_line:
                    f.write(self._txt + '\n')
                f.write(l)


def generate_diff_if_needed(path):
    if is_skipped(path):
        return None

    matcher = find_matcher(path)
    if not matcher:
        logging.error("Did not find valid matcher for file: {}".format(path))
        return None

    # Read the file contents.
    contents = None
    with open(path, 'r') as f:
        contents = f.read()

    # If  the file already has SPDX, we just skip it. This implies we already added
    # the required license header.
    if has_spdx(contents):
        return None

    # Keep popping up directories util we find a LICENSE file.
    # We will use that as the default license for this file.
    (subpath, remain) = os.path.split(path)
    license = None
    while True:
        if remain == '':
            break

        license_file = os.path.join(subpath, 'LICENSE')
        if os.path.isfile(license_file):
            license = get_spdx_from_license(license_file)
            break
        (subpath, remain) = os.path.split(subpath)

    # Some files have lines that need to occur before the license. This includes
    # go build constraints, and shell lines.
    content_lines = contents.split('\n')
    offset = 0

    if 'skip_lines' in matcher:
        for s in matcher['skip_lines']:
            if s.match(content_lines[offset]) and offset < len(content_lines):
                offset += 1

    # We check and make sure the exact copy of the license occurs after skipping the offset.
    contents_with_offset = '\n'.join(content_lines[offset + 1:])
    expected_license = matcher['wrapper'](license_by_spdx[license])
    if not contents_with_offset.startswith(expected_license):
        license_text = expected_license
        # If we aren't the first line then add a space between the license and other lines.
        # Since we split, '' means there was a \n.
        if offset > 0:
            if content_lines[offset - 1] != '':
                license_text = '\n' + license_text
        if offset < len(content_lines):
            if content_lines[offset] != '':
                license_text = license_text + '\n'
        return AddLicenseDiff(path, offset + 1, license_text)


def main():
    args = parse_args()

    path = args.f
    autofix = args.a
    if os.path.isfile(path):
        diff = generate_diff_if_needed(path)
        if diff is not None:
            if autofix:
                diff.fix(path)
            else:
                print(diff.phabricator())
    else:
        logging.fatal('-f argument is required and needs to be a file')


if __name__ == '__main__':
    main()
