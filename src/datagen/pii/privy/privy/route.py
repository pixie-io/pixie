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
    def __init__(self, file_writers, analyzer):
        self.file_writers = file_writers
        self.conversions = {
            "json": (json.dumps, {"default": "str"}),
            "xml": (dicttoxml, {}),
            # todo @benkilimnik protobuf conversion
            # todo @benkilimnik sql conversion
        }
        self.analyzer = analyzer

    def write_payload_to_csv(self, payload, has_pii, pii_types):
        if not payload or "null" in payload.values():
            return
        if pii_types:
            pii_types, categories = zip(*pii_types)
            self.analyzer.update_pii_counters(pii_types, categories)
        else:
            pii_types, categories = [], []
        has_pii = str(int(has_pii))
        pii_types = ",".join(set(pii_types))
        categories = ",".join(set(categories))
        for writer in self.file_writers:
            converter, kwargs = self.conversions.get(
                writer.generate_type, None)
            converted_payload = converter(payload, **kwargs)
            row = [converted_payload, has_pii, pii_types, categories]
            writer.csv_writer.writerow(row)
        self.analyzer.update_payload_counts()
