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

import os
import pathlib
import unittest

from privy.providers.english_us import English_US
from privy.providers.german_de import German_DE
from privy.tests.utils import (generate_one_api_spec, get_delimited,
                               read_generated_csv)


class TestSQLGenerator(unittest.TestCase):
    def setUp(self):
        self.provider_regions = [English_US(), German_DE()]
        lufthansa_openapi = os.path.join(os.path.dirname(__file__), "openapi.json")
        self.api_specs_folder = pathlib.Path(lufthansa_openapi).parents[0]

    def test_query_builder(self):
        for multi_threaded in [False, True]:
            for region in self.provider_regions:
                file = generate_one_api_spec(self.api_specs_folder, region, multi_threaded, "sql")
                payloads, pii_types_per_payload = read_generated_csv(file, generate_type="sql")
                # check that pii_type column values match pii_types present in the request payload
                query_types = {
                    "select": ["ORDER BY", "WHERE", "LIMIT", "GROUP BY", "CASE"],
                    "insert": ["columns"],
                    "update": ["where"],
                }
                # test that query types are correctly generated
                for payload, pii_types in zip(payloads, pii_types_per_payload):
                    # check that a valid query was generated
                    if payload.startswith("SELECT"):
                        # check that select has at least one valid subtype
                        self.assertTrue(
                            any([subtype in payload for subtype in query_types['select']]),
                        )
                    # check if at least one of the pii types is in the query
                    # exclude inserts/updates, since they may not have the columns (params/pii_types) shown
                    if not payload.startswith("UPDATE") and not payload.startswith("INSERT"):
                        if pii_types:
                            for pii_type in pii_types:
                                delimited = get_delimited(pii_type)
                                self.assertTrue(
                                    any([delim_pii_type.lower() == pii_type.lower() for delim_pii_type in delimited])
                                )
                file.close()


if __name__ == "__main__":
    unittest.main()
