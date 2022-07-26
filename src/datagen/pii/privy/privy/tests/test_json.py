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
import pandas as pd
import numpy as np
from privy.chosen_providers import Providers
from privy.payload import PayloadGenerator
from privy.hooks import SchemaHooks


class TestPayloadGenerator(unittest.TestCase):
    def setUp(self):
        self.providers = Providers()
        self.hook = SchemaHooks()
        self.lufthansa_openapi = os.path.join(os.path.dirname(__file__), "openapi.json")

    def test_parse_http_methods(self):
        for region in self.providers.get_regions():
            # in-memory file-like object
            file = io.StringIO()
            csvwriter = csv.writer(file, quotechar="|")
            self.payload_generator = PayloadGenerator(
                pathlib.Path(self.lufthansa_openapi).parents[0], csvwriter, "json", 0.25, 0.03
            )
            self.payload_generator.generate_payloads()
            file.seek(0)

            # no header added to data in test since that occurs in run / generate.py
            df = pd.read_csv(file, engine="python", quotechar="|", header=None)
            df.rename(
                columns={0: "payload", 1: "has_pii", 2: "pii_types"}, inplace=True
            )
            # check that pii_type column values match pii_types present in the request payload
            payload_params = [
                json.loads(r).keys() if r is not np.nan else {} for r in df["payload"]
            ]
            pii_types_per_payload = [
                p.split(",") if p is not np.nan else [] for p in df["pii_types"]
            ]
            for params, pii_types in zip(payload_params, pii_types_per_payload):
                for param in params:
                    label_pii_tuple = region.get_pii(param)
                    if label_pii_tuple:
                        pii_label, _ = label_pii_tuple
                        if pii_label:
                            self.assertTrue(pii_label in pii_types)
                        else:
                            self.assertTrue(pii_label not in pii_types)
            file.close()


if __name__ == "__main__":
    unittest.main()
