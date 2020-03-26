#!/usr/bin/python
# Script aimed to combining multiple license files into a single one.
import argparse
from collections import OrderedDict
import json
import urllib.request
import urllib.error


def read_file(fname):
    with open(fname, encoding='utf-8') as f:
        return f.read()


def get_package_name(license_fname):
    directory_name = license_fname.split('/')[1]
    # If no underscores, this is the full name.
    if '_' not in directory_name:
        return directory_name

    splits = directory_name.split('_')
    if splits[0] == "com":
        splits = splits[1:]
    if splits[0] == "github":
        splits = splits[1:]

    assert len(splits) >= 2, directory_name
    return "{}/{}".format(splits[0], '-'.join(splits[1:]))


def does404(url):
    try:
        _ = urllib.request.urlopen(url)
    except urllib.error.HTTPError as e:
        return e.code == 404
    return False


def get_package_url(license_fname):
    directory_name = license_fname.split('/')[1]

    # If no underscores, then we cannot resolve the url.
    if '_' not in directory_name:
        return None

    github_url = "https://github.com/{}".format(
        get_package_name(license_fname))
    if not does404(github_url):
        return github_url

    return None


def package_license(license_fname):
    license_dict = {
        "name": get_package_name(license_fname),
        "content": read_file(license_fname)
    }
    url = get_package_url(license_fname)
    if url:
        license_dict['url'] = url

    return license_dict


def main():
    parser = argparse.ArgumentParser(
        description='Combines the license files into a map to be used.')
    parser.add_argument('out_fname', type=str,
                        help='Out filename.')
    parser.add_argument('filenames', metavar='N', type=str, nargs='+',
                        help='the filenames of the license files to combine.')
    args = parser.parse_args()

    license_mapping = OrderedDict()
    for license_fname in args.filenames:
        license_mapping[license_fname] = package_license(license_fname)

    with open(args.out_fname, 'w') as f:
        json.dump(license_mapping, f, indent=4)


if __name__ == "__main__":
    main()
