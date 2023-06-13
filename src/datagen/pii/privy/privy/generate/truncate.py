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
import csv
import logging
import os
import pathlib
import shutil
from tempfile import NamedTemporaryFile


def parse_args():
    """Perform command-line argument parsing."""

    parser = argparse.ArgumentParser(
        description="Truncate PII data in a CSV file to a maximum number of characters per line.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument(
        "--input",
        "-i",
        required=True,
        help="Absolute path to input data.",
    )

    parser.add_argument(
        "--max_chars_per_line",
        "-m",
        type=int,
        required=False,
        default=500,
        help="Number of characters to keep per line before truncating.",
    )

    parser.add_argument(
        "--replace",
        "-r",
        action='store_true',
        required=False,
        default=False,
        help="Truncate in-place, overwriting the input file.",
    )

    parser.add_argument(
        "--out",
        "-o",
        required=False,
        default=os.path.join(os.path.dirname(__file__), os.pardir),
        help="Absolute path to where truncated data will be stored. By default, saves to bazel cache for this runtime.",
    )

    return parser.parse_args()


def truncate_file(source, dest, max_chars_per_line):
    reader = csv.reader(source, quotechar='|')
    writer = csv.writer(dest, quotechar='|')
    for row in reader:
        payload = row[0]
        if len(payload) > max_chars_per_line:
            payload = f"{row[0][:max_chars_per_line]}... [TRUNCATED]"
        has_pii = row[1]
        pii_types = row[2]
        writer.writerow([payload, has_pii, pii_types])


def truncate(input_csv, output_csv, max_chars_per_line, replace):
    if replace or input_csv == output_csv:
        # create temporary file to store truncated data
        tempfile = NamedTemporaryFile('w+t', delete=False)
        with open(input_csv, 'r') as source, tempfile:
            truncate_file(source, tempfile, max_chars_per_line)
        # replace original file with truncated file
        shutil.move(tempfile.name, input_csv)
    else:
        # truncate and store as copy to full path given by output_csv
        with open(input_csv, 'r') as source, open(output_csv, 'w') as dest:
            truncate_file(source, dest, max_chars_per_line)


def main(args):
    # check that parent folder exists for output and that input file exists
    if pathlib.Path(args.out).parent.exists():
        logging.getLogger("privy").error(f"Truncating {args.input} and saving to {args.out}")
        truncate(args.input, args.out, args.max_chars_per_line, args.replace)
    else:
        logging.getLogger("privy").error(f"Parent folder for output or input file does not exist: {args.out}")


if __name__ == '__main__':
    args = parse_args()
    main(args)
