#!/usr/bin/python3
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
