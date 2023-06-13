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

from privy.generate.utils import PrivyFileType
from privy.providers.english_us import English_US
from privy.providers.german_de import German_DE
from privy.tests.utils import generate_one_api_spec, read_generated_csv


class TestPayloadGenerator(unittest.TestCase):
    def setUp(self):
        self.regions = [English_US(), German_DE()]
        lufthansa_openapi = os.path.join("openapi.json")
        self.api_specs_folder = pathlib.Path(lufthansa_openapi).parents[0]

    def test_parse_http_methods(self):
        for multi_threaded in [False, True]:
            for region in self.regions:
                file = generate_one_api_spec(self.api_specs_folder, region, multi_threaded, "json",
                                             PrivyFileType.PAYLOADS)
                payload_params, pii_types_per_payload = read_generated_csv(
                    file)
                # check that pii_type column values match pii_types present in the request payload
                for params, pii_types in zip(payload_params, pii_types_per_payload):
                    for param in params:
                        pii = region.get_pii_provider(param)
                        if pii:
                            self.assertTrue(pii.template_name in pii_types)
                file.close()


if __name__ == "__main__":
    unittest.main()
