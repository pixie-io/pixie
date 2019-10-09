#!/usr/bin/python
# Script aimed to combining multiple license files into a single one.
import argparse
from collections import OrderedDict
import json
import urllib2

import re


def get_file_at_url(url):
    try:
        r = urllib2.urlopen(url)
    except urllib2.HTTPError, e:
        print(url)
        raise e
    return r.read()


def read_file(fname):
    with open(fname) as f:
        return f.read()


def github_username_and_project(license_url):
    res = urllib2.urlparse.urlparse(license_url)
    matches = res.path[1:].split('/')
    assert len(matches) >= 2, "should be 2 or greater matches not {}".format(
        len(matches))
    user = matches[0]
    project = matches[1]
    return user, project


def raw_github_url_to_github_repo(raw_github_url):
    ''' Converts raw github url to the containing github repo '''
    username, project = github_username_and_project(raw_github_url)
    return 'https://github.com/{username}/{project}'.format(username=username, project=project)


def package_license(package_name, license_url, override_license_contents):
    if package_name not in override_license_contents:
        contents = get_file_at_url(license_url)
    else:
        contents = override_license_contents[package_name]

    username, project = github_username_and_project(license_url)
    return {
        "name": project,
        "content": contents,
        "url": raw_github_url_to_github_repo(license_url)
    }


def main():
    parser = argparse.ArgumentParser(
        description='Combines the license files into a map to be used.')
    parser.add_argument('out_fname', type=str,
                        help='The markdown file to write.')
    parser.add_argument('json_file', metavar='N', type=str,
                        help='the filename of the json mappings to combine')
    args = parser.parse_args()

    with open(args.json_file) as f:
        manual_license_map = json.load(f, object_pairs_hook=OrderedDict)

    license_src_key = "license_sources"
    license_content_key = "license_contents"
    print(manual_license_map.keys())

    license_sources = manual_license_map[license_src_key]
    override_license_contents = manual_license_map[license_content_key]
    license_contents = OrderedDict()
    for package_name, license_url in license_sources.items():
        license_contents[package_name] = package_license(
            package_name, license_url, override_license_contents)

    with open(args.out_fname, 'w') as f:
        json.dump(license_contents, f, indent=4)


if __name__ == "__main__":
    main()
