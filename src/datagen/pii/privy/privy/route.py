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
from dicttoxml import dicttoxml


class PayloadRoute:

    def __init__(self, csvwriter, generate_type):
        self.csvwriter = csvwriter
        self.generate_type = generate_type

    def write_payload_to_csv(self, payload):
        if not payload or "null" in payload.values():
            return
        converted = {
            "json": json.dumps(payload, default=str),
            "xml": dicttoxml(payload),
            # todo @benkilimnik protobuf conversion
            # todo @benkilimnik sql conversion
        }.get(self.generate_type, None)
        if not converted:
            return
        payload = [converted]
        self.csvwriter.writerow(payload)
