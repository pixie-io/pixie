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

import csv
import io
import json
from collections import defaultdict

import numpy as np
import pandas as pd

from privy.generate.utils import PrivyFileType, PrivyWriter
from privy.payload import PayloadGenerator


class PrivyArgs:
    def __init__(self, args):
        # deconstruct args into dict, setting each as instance attribute
        for key in args:
            setattr(self, key, args[key])


def generate_one_api_spec(api_specs_folder, region, multi_threaded, generate_type, file_type=PrivyFileType.PAYLOADS,
                          logging="debug", num_additional_pii_types=6, equalize_pii_distribution_to_percentage=50,
                          timeout=400, fuzz=False) -> io.StringIO:
    file = io.StringIO()
    args = {
        "generate_types": generate_type,
        "region": region,
        "logging": logging,
        "multi_threaded": multi_threaded,
        "num_additional_pii_types": num_additional_pii_types,
        "equalize_pii_distribution_to_percentage": equalize_pii_distribution_to_percentage,
        "timeout": timeout,
        "fuzz_payloads": fuzz,
        "file_type": file_type,
        "spans_per_template": 10,
        "ignore_spec": ["stripe.com"],
    }
    args = PrivyArgs(args)
    file_writers = defaultdict(list)
    csv_writer = csv.writer(file, quotechar="|")
    file_writers[args.generate_types] = [
        PrivyWriter(args.file_type, file, csv_writer)]
    payload_generator = PayloadGenerator(api_specs_folder, file_writers, args)
    payload_generator.generate_payloads()
    file.seek(0)
    return file


def read_generated_csv(file, generate_type="json"):
    df = pd.read_csv(file, engine="python", quotechar="|", header=None)
    df.rename(
        columns={0: "payload", 1: "has_pii", 2: "pii_types"}, inplace=True
    )
    pii_types_per_payload = [
        p.split(",") if p is not np.nan else [] for p in df["pii_types"]
    ]
    if generate_type == "json":
        payload_params = [
            json.loads(r).keys() if r is not np.nan else {} for r in df["payload"]
        ]
        return payload_params, pii_types_per_payload
    elif generate_type == "sql":
        payloads = [
            r if r is not np.nan else {} for r in df["payload"]
        ]
        return payloads, pii_types_per_payload


def get_delimited(label):
    """Return list of versions of input label with different delimiters"""
    label_delimited = [
        label,
        label.replace(" ", "-"),
        label.replace(" ", "_"),
        label.replace(" ", "__"),
        label.replace(" ", "."),
        label.replace(" ", ":"),
        label.replace(" ", ""),
    ]
    return label_delimited
