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
import tarfile
from collections import defaultdict
from pathlib import Path
from typing import Any, Tuple

import requests

from privy.generate.utils import (PrivyFileType, PrivyWriter, check_percentage,
                                  check_positive)
from privy.payload import PayloadGenerator
from privy.providers.english_us import English_US
from privy.providers.german_de import German_DE


def parse_args():
    """Perform command-line argument parsing."""

    parser = argparse.ArgumentParser(
        description="Synthetic protocol trace & PII data generator",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--generate_types",
        "-g",
        required=False,
        choices=[
            "json",
            "sql",
            "proto",
            "xml",
            "html",
        ],
        nargs='+',
        default=["json"],
        help="Which dataset to generate. Can select multiple e.g. json sql proto xml",
    )

    parser.add_argument(
        "--region",
        "-r",
        required=False,
        choices=[
            "english_us",
            "german_de",
        ],
        default="english_us",
        help="""Which language/region specific providers to use for PII generation.""",
    )

    parser.add_argument(
        "--logging",
        "-l",
        required=False,
        choices=[
            "debug",
            "info",
            "warning",
            "error",
        ],
        default="info",
        help="logging level: debug, info, warning, error",
    )

    parser.add_argument(
        "--out_folder",
        "-o",
        required=False,
        default=Path(__file__).parent,
        help="Absolute path to output folder. By default, saves to bazel cache for this runtime.",
    )

    parser.add_argument(
        "--api_specs",
        "-a",
        required=False,
        default=Path(__file__).parent,
        help="Absolute path to folder to download openapi specs into. Privy checks if this folder already exists.",
    )

    parser.add_argument(
        "--fake_persons_file_path",
        "-fp",
        required=False,
        default="",
        help="Absolute path to file containing fake person data downloaded from fakenamegenerator.com.",
    )

    parser.add_argument(
        "--multi_threaded",
        "-m",
        action="store_true",
        required=False,
        default=False,
        help="Generate data multithreaded",
    )

    parser.add_argument(
        "--num_additional_pii_types",
        "-n",
        required=False,
        default=6,
        type=check_positive,
        help="""Upper bound for the number of PII types to generate
        when inserting additional PII into sensitive payloads. E.g. 6""",
    )

    parser.add_argument(
        "--equalize_pii_distribution_to_percentage",
        "-e",
        required=False,
        default=50,
        type=check_percentage,
        help="""Equalize distribution of PII in the dataset to the given percentage by generating additional
        PII payloads for pii types with the lowest count. To disable, set to 0.""",
    )

    parser.add_argument(
        "--timeout",
        "-t",
        required=False,
        default=400,
        type=check_positive,
        help="""Timeout (in seconds) after which data generation for the current openAPI descriptor will
        be halted. Very large descriptors tend to slow down data generation and skew the output dataset,
        so we apply a uniform timeout to each."""
    )

    parser.add_argument(
        "--pii_types",
        "-p",
        nargs='+',
        required=False,
        help="PII types to match and generate. If not specified, all available PII types will be matched.",
    )

    parser.add_argument(
        "--fuzz_payloads",
        "-f",
        action="store_true",
        required=False,
        default=False,
        help="""Fuzz payloads by removing characters.""",
    )

    parser.add_argument(
        "--spans_per_template",
        "-s",
        required=False,
        type=check_positive,
        default=10,
        help="""Number of (non-)PII spans (NER-compatible, token-wise labeled samples)
        to generate per unique payload template.""",
    )

    parser.add_argument(
        "--ignore_spec",
        "-ig",
        nargs='+',
        default=["stripe.com"],
        required=False,
        help="OpenAPI specs to ignore. If not specified, all specs will be matched.",
    )

    return parser.parse_args()


def generate(args: argparse.Namespace, out_files: dict[str, Tuple[Any]], api_specs_folder: Path) -> None:
    headers = ["payload", "has_pii", "pii_types"]
    file_writers = defaultdict(list)
    try:
        for generate_type, out_files in out_files.items():
            for file in out_files:
                file_type, file_path = file[0], file[1]
                Path(file_path).parent.mkdir(parents=True, exist_ok=True)
                open_file = open(file_path, 'w')
                csv_writer = csv.writer(open_file, quotechar="|")
                if file_type == PrivyFileType.PAYLOADS:
                    csv_writer.writerow(headers)
                file_writers[generate_type].append(PrivyWriter(file_type, open_file, csv_writer))
        api_specs_folder = api_specs_folder / "APIs"
        payload_generator = PayloadGenerator(api_specs_folder, file_writers, args)
        payload_generator.generate_payloads()
    # ------ Close File Handles --------
    finally:
        for writers in file_writers.values():
            for privy_writer in writers:
                privy_writer.open_file.close()


def main(args):
    # ------ Logging --------
    numeric_level = getattr(logging, args.logging.upper(), None)
    # set root logging level
    logging.basicConfig(level=logging.WARNING)
    log = logging.getLogger("privy")
    log.setLevel(numeric_level)

    # ------ Load OpenAPI directory -------
    log.info(f"Checking if openapi-directory exists in {args.api_specs}")
    api_specs_folder = Path(
        args.api_specs) / "openapi-directory-ea4a924b870ca4f6d687809fa7891cccc0d19085"
    if not api_specs_folder.exists():
        log.info("Not found. Downloading...")
        commit_hash = "ea4a924b870ca4f6d687809fa7891cccc0d19085"
        openapi_directory_link = f"https://github.com/APIs-guru/openapi-directory/archive/{commit_hash}.tar.gz"
        with requests.get(openapi_directory_link, stream=True) as rx, tarfile.open(fileobj=rx.raw, mode="r:gz") as tar:
            tar.extractall(api_specs_folder.parent)

    # ------- Choose Providers --------
    args.region = {
        "english_us": English_US,
        "german_de": German_DE,
    }.get(args.region)(pii_types=args.pii_types)

    # ------ Initialize File Handles --------
    out_files = {}
    for generate_type in args.generate_types:
        log.info(f"Generating {generate_type.upper()} dataset")
        payloads_file = Path(args.out_folder) / "data" / \
            f"{generate_type.lower()}-payloads.csv"
        templates_file = Path(args.out_folder) / "data" / \
            f"{generate_type.lower()}-templates.txt"
        spans_file = Path(args.out_folder) / "data" / \
            f"{generate_type.lower()}-spans.json"
        out_files[generate_type] = [(PrivyFileType.PAYLOADS, payloads_file), (PrivyFileType.TEMPLATES, templates_file),
                                    (PrivyFileType.SPANS, spans_file)]
    generate(args, out_files, api_specs_folder)


if __name__ == "__main__":
    args = parse_args()
    main(args)
