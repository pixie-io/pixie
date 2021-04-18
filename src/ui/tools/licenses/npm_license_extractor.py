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

# Parses the output of npm license-checker into the license format we use to generate our OSS
# license notice page.
import argparse
from collections import defaultdict, OrderedDict
import json
import re

# Constant key values
LICENSES_KEY = 'licenses'
PATH_KEY = 'path'
REPOSITORY_KEY = 'repository'
LICENSE_FILE_KEY = 'licenseFile'


# This isn't exhaustive, just a quick easy way to pick one of many licenses.
# Larger is better.
PREFERRED_LICENSES_DICT = defaultdict(int, {
    'CC0-1.0': 10,
    'Apache-2.0': 9,
    'MIT': 8,
})


GITHUB_MATCHER = re.compile("github.com[:/](.*?/[^/]*)")


def get_name(project_name: str, license_details: dict) -> str:
    if REPOSITORY_KEY in license_details:
        matches = GITHUB_MATCHER.findall(license_details[REPOSITORY_KEY])
        if len(matches) > 0:
            return matches[0]

    # print('No repo, falling back to path for project {}'.format(project_name))
    path = license_details[PATH_KEY]
    splits = path.split('/node_modules/')
    if len(splits) > 1:
        return splits[1]

    # print('No path, falling back to project name for project {}'.format(project_name))
    splits = project_name.split('@')
    return splits[0]


def get_clean_repo(project_name: str, license_details: dict) -> str:
    if REPOSITORY_KEY not in license_details:
        return ''

    matches = GITHUB_MATCHER.findall(license_details[REPOSITORY_KEY])
    if len(matches) > 0:
        return "https://github.com/{}".format(matches[0])


def get_spdx_id(repo: str, details: dict) -> str:
    if LICENSES_KEY not in details:
        # print('Missing licenses ids for repo {}'.format(repo))
        return ''

    licenses = details[LICENSES_KEY]
    licenses = licenses.replace('AND', 'and')
    licenses = licenses.replace('OR', 'or')

    license_options = licenses.split(' or ')
    if len(license_options) == 1:
        return license_options[0]

    # Trim the leading paren
    license_options[0] = license_options[0][1:]
    # Trim the trailing paren
    license_options[-1] = license_options[-1][:-1]

    best_license = license_options[0]
    best_license_score = PREFERRED_LICENSES_DICT[best_license]
    for lic in license_options:
        if PREFERRED_LICENSES_DICT[lic] > best_license_score:
            best_license = lic
            best_license_score = PREFERRED_LICENSES_DICT[lic]

    if best_license_score == 0:
        # print('Couldn\'t determine a license to pick for repo {}'.format(repo))
        return ' or '.join(license_options)

    return best_license


def get_license_text(repo: str, details: dict) -> str:
    if LICENSE_FILE_KEY not in details:
        # print('Missing license file for repo {}'.format(repo))
        return ''

    with open(details[LICENSE_FILE_KEY], encoding='utf-8') as f:
        return f.read()


class Dependency(dict):
    def __init__(self) -> None:
        dict.__init__(self)

    # Keep these in sync with tools/licenses/fetch_licenses.go
    def set_name(self, name: str) -> None:
        dict.__setitem__(self, "name", name)

    def set_url(self, url: str) -> None:
        dict.__setitem__(self, "url", url)

    def set_spdx_id(self, spdx_id: str) -> None:
        dict.__setitem__(self, "spdxID", spdx_id)

    def set_license_text(self, license_text: str) -> None:
        dict.__setitem__(self, "licenseText", license_text)


def main():
    parser = argparse.ArgumentParser(
        description='Combines the license files into a map to be used.')
    parser.add_argument('--input', type=str,
                        help='The output of license-checker to read in.')
    parser.add_argument('--output', type=str,
                        help='The output json file to write.')
    args = parser.parse_args()

    with open(args.input, encoding='utf-8') as f:
        npm_license_map = json.load(f, object_pairs_hook=OrderedDict)

    npm_unique = OrderedDict({})
    for project_name, license_details in npm_license_map.items():
        name = get_name(project_name, license_details)
        repo = get_clean_repo(project_name, license_details)
        if repo != '':
            if repo not in npm_unique:
                npm_unique[repo] = Dependency()
            contents = npm_unique[repo]
            contents.set_url(repo)
        else:
            if name not in npm_unique:
                npm_unique[name] = Dependency()
            contents = npm_unique[name]
        contents.set_name(name)
        contents.set_spdx_id(get_spdx_id(project_name, license_details))
        contents.set_license_text(get_license_text(project_name, license_details))

    with open(args.output, 'w') as f:
        json.dump(list(npm_unique.values()), f, indent=4)


if __name__ == '__main__':
    main()
