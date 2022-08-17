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

import json
import unittest
import os
import pathlib
import csv
import io
from collections import namedtuple
import pandas as pd
import numpy as np
from privy.chosen_providers import Providers
from privy.payload import PayloadGenerator


class PrivyArgs:
    def __init__(self, args):
        # deconstruct args into dict, setting each as instance attribute
        for key in args:
            setattr(self, key, args[key])


class TestPayloadGenerator(unittest.TestCase):
    def setUp(self):
        self.providers = Providers()
        lufthansa_openapi = os.path.join("openapi.json")
        self.api_specs_folder = pathlib.Path(lufthansa_openapi).parents[0]

    def test_parse_http_methods(self):
        for multi_threaded in [False, True]:
            for region in self.providers.get_regions():
                # in-memory file-like object
                file = io.StringIO()
                args = {
                    "generate_types": "json",
                    "region": region,
                    "logging": "debug",
                    "multi_threaded": multi_threaded,
                    "insert_pii_percentage": 0.6,
                    "insert_label_pii_percentage": 0.05,
                    "timeout": 400,
                }
                args = PrivyArgs(args)

                PrivyWriter = namedtuple("PrivyWriter", ["generate_type", "open_file", "csv_writer"])
                file_writers = []
                csv_writer = csv.writer(file, quotechar="|")
                file_writers.append(PrivyWriter(args.generate_types, file, csv_writer))
                payload_generator = PayloadGenerator(self.api_specs_folder, file_writers, args)
                payload_generator.generate_payloads()
                file.seek(0)

                # no header added to data in test since that occurs in run / generate.py
                df = pd.read_csv(file, engine="python", quotechar="|", header=None)
                df.rename(
                    columns={0: "payload", 1: "has_pii", 2: "pii_types", 3: "categories"}, inplace=True
                )
                # check that pii_type column values match pii_types present in the request payload
                payload_params = [
                    json.loads(r).keys() if r is not np.nan else {} for r in df["payload"]
                ]
                pii_types_per_payload = [
                    p.split(",") if p is not np.nan else [] for p in df["pii_types"]
                ]
                categories_per_payload = [
                    c.split(",") if c is not np.nan else [] for c in df["categories"]
                ]
                for params, pii_types, categories in zip(payload_params, pii_types_per_payload, categories_per_payload):
                    for param in params:
                        pii = region.get_pii(param)
                        if pii:
                            self.assertTrue(pii.label in pii_types)
                            self.assertTrue(pii.category in categories)
                file.close()


if __name__ == "__main__":
    unittest.main()
