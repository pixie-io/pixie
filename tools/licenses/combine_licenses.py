#!/usr/bin/python3

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
import json


def main():
    parser = argparse.ArgumentParser(
        description='Combines the license files into a map to be used.')
    parser.add_argument('input', metavar='N', type=str, nargs='+',
                        help='The filenames of the license files to combine.')
    parser.add_argument('--output', type=str,
                        help='Out filename.')
    args = parser.parse_args()

    all_licenses = []
    for input_file in args.input:
        with open(input_file, encoding='utf-8') as f:
            all_licenses.extend(json.load(f))

    with open(args.output, 'w') as f:
        json.dump(all_licenses, f, indent=4)


if __name__ == '__main__':
    main()
