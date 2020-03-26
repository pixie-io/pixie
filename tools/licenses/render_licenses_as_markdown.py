#!/usr/bin/python
# Script aimed to combining multiple license files into a single one.
import argparse
from collections import OrderedDict
import json


def read_file(fname):
    with open(fname, encoding='utf-8') as f:
        return f.read()


def format_markdown(license_dictionary, seen_names):

    assert 'name' in license_dictionary
    assert 'content' in license_dictionary

    name = license_dictionary['name']
    if name in seen_names:
        return ''

    seen_names.add(name)

    body = ['```\n{}\n```'.format(license_dictionary['content'])]
    if 'url' in license_dictionary:
        body = ["[Project Link]({url})".format(
            url=license_dictionary['url'])] + body
    license_md = "# {name}\n{body}\n".format(
        name=name, body='\n'.join(body))
    return license_md


def main():
    parser = argparse.ArgumentParser(
        description='Combines the license files into a map to be used.')
    parser.add_argument('out_fname', type=str,
                        help='The markdown file to write.')
    parser.add_argument('json_files', metavar='N', type=str, nargs='+',
                        help='the filenames of the json_mappings_to_combine')
    args = parser.parse_args()

    license_mapping = OrderedDict()
    for license_fname in args.json_files:
        print("Running", license_fname)
        with open(license_fname, encoding='utf-8') as f:
            license_mapping.update(json.load(f, object_pairs_hook=OrderedDict))

    failed_licenses = []
    seen_names = set()
    with open(args.out_fname, 'w', encoding='utf-8') as f:
        for license_name, license_contents in license_mapping.items():
            try:
                f.write(format_markdown(license_contents, seen_names))
            except UnicodeEncodeError:
                failed_licenses.append(license_name)

    print("Failed licenses:")
    for l in failed_licenses:
        print("\t{}".format(l))


if __name__ == "__main__":
    main()
