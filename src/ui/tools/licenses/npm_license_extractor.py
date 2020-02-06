#!/usr/bin/python
# Parses the output of npm license-checker into the license format we use to generate our OSS
# license notice page.
import argparse
from collections import OrderedDict
import json
import urllib2
import re
import os
import prepared_licenses

# Constant key values
REPOSITORY_KEY = 'repository'
PUBLISHER_KEY = 'publisher'
LICENSES_KEYS = 'licenses'


def get_file_at_url(url):
    r = urllib2.urlopen(url)
    return r.read()


def read_file(fname):
    with open(fname) as f:
        return f.read()


def get_package_name_from_url(license_url):
    matches = re.findall("github.com[:/](.*?/[^/]*)", license_url)
    if len(matches) == 0:
        return None
    return matches[0]


OWNER_MAPPING_DICT = {
    "facebook": "Facebook, Inc. and its affiliates",
}


def get_project_name(project_name, license_details):

    if REPOSITORY_KEY not in license_details:
        return project_name

    new_project_name = get_package_name_from_url(
        license_details[REPOSITORY_KEY])
    if not new_project_name:
        return project_name

    return new_project_name


def special_owner_mapping(name):
    if name in OWNER_MAPPING_DICT:
        return OWNER_MAPPING_DICT[name]
    return name


def _find_owner_impl(license_details):
    if PUBLISHER_KEY in license_details:
        owner = license_details[PUBLISHER_KEY]
        return owner.encode('ascii', errors='ignore')

    if REPOSITORY_KEY not in license_details:
        return ''

    # work around to Guess at who the owner is.
    repository = license_details[REPOSITORY_KEY]
    matches = re.findall('github.com/(.*?)/', repository)
    if len(matches) == 0:
        return ""
    return matches[0]


def find_owner(license_details):
    owner = _find_owner_impl(license_details)
    return special_owner_mapping(owner)


def make_license(license_details):
    if LICENSES_KEYS not in license_details:
        print(license_details['path'], 'no license options found')
        return ''

    licenses = license_details[LICENSES_KEYS]
    owner = find_owner(license_details)

    if 'MIT' in licenses:
        return prepared_licenses.mit_license(year(), owner)
    elif 'Apache-2.0' in licenses or 'Apache License, Version 2.0' in licenses:
        return prepared_licenses.APACHE_LICENSE
    elif 'ISC' in licenses:
        return prepared_licenses.isc_license(year(), owner)
    elif 'GPL-3.0' in licenses:
        return prepared_licenses.gpl3_license(year(), owner)
    elif 'BSD-2' in licenses:
        return prepared_licenses.bsd2_license(year(), owner)
    elif 'BSD' in licenses:
        return prepared_licenses.bsd3_license(year(), owner)
    elif 'Public Domain' in licenses:
        return prepared_licenses.public_domain_license(year(), owner)
    elif 'CC0' in licenses:
        return prepared_licenses.cc0_license(year(), owner)
    elif 'CC-BY-3.0' in licenses:
        return prepared_licenses.cc3_license(year(), owner)

    print(license_details['path'], 'no license options found', licenses)
    return ''


def year():
    from datetime import datetime
    return datetime.today().year


def get_license(project_name, license_details, override_license):
    if project_name in override_license:
        return override_license[project_name]
    if 'licenseFile' not in license_details:
        return make_license(license_details)

    license_file = license_details['licenseFile']

    # Override the suggested license because README files tend to break the rendering.
    if "readme" in os.path.basename(license_file).lower():
        return make_license(license_details)

    # If license file is something else, might as well try to render it.
    return read_file(license_file)


def process_license(project_name, license_details, override_license={}):
    '''
    Processes the license into the json object used by the final setup to interpret everything.
    '''
    license_dict = {
        "name": get_project_name(project_name, license_details),
        "content": get_license(project_name, license_details, override_license)
    }

    if 'licenses' in license_details:
        license_dict['type'] = license_details['licenses']
    if 'repository' in license_details:
        license_dict['url'] = license_details['repository']
    return license_dict


def format_project_name(project_name):
    return re.sub("(?<!^)@.*", '', project_name)


def main():
    parser = argparse.ArgumentParser(
        description='Combines the license files into a map to be used.')
    parser.add_argument('out_fname', type=str,
                        help='The markdown file to write.')
    parser.add_argument('npm_json_file', metavar='N', type=str,
                        help='the output of license-checker to read in ')
    parser.add_argument('--override_license',
                        help='file containing json that overrides package pointers.')
    parser.add_argument('--pl_pkg_name', default="pl_ui",
                        help="The name of the pl ui package to ignore")
    args = parser.parse_args()

    with open(args.npm_json_file) as f:
        npm_license_map = json.load(f, object_pairs_hook=OrderedDict)

    override_license = {}
    if args.override_license:
        with open(args.override_license) as f:
            override_license = json.load(f)

    license_contents = OrderedDict({})
    failed_packages = []
    for unformatted_project_name, license_details in npm_license_map.items():
        # Because license names can have versions, we only take one of the versions.
        project_name = format_project_name(unformatted_project_name)

        # Ignore the license that maps to the PL pkg.
        if project_name == args.pl_pkg_name:
            continue

        # Skip license if it's already been added.
        if project_name in license_contents:
            continue

        # Save the license for output.
        license_contents[project_name] = process_license(
            project_name, license_details, override_license)

    if failed_packages:
        print('Failed packges')
        print(json.dumps(failed_packages, indent=4))

    with open(args.out_fname, 'w') as f:
        json.dump(license_contents, f, indent=4)


if __name__ == "__main__":
    main()
