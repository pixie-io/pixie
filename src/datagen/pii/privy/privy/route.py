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
    def __init__(self, file_writers, analyzer, args):
        self.file_writers = file_writers
        self.conversions = {
            "json": (json.dumps, {"default": "str"}),
            "xml": (dicttoxml, {}),
            # todo @benkilimnik protobuf conversion
            # todo @benkilimnik sql conversion
        }
        self.args = args
        self.analyzer = analyzer
        self.fuzzer = PayloadFuzzer()

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
            if self.args.fuzz_payloads:
                self.write_fuzzed_payloads(row, writer)
        self.analyzer.update_payload_counts()

    def write_fuzzed_payloads(self, row, writer):
        for fuzzed_payload in self.fuzzer.fuzz_payload(row[0], writer.generate_type):
            row[0] = fuzzed_payload
            writer.csv_writer.writerow(row)


class PayloadFuzzer:
    def __init__(self):
        self.fuzzers = {
            "json": self.fuzz_json_payload,
            "xml": self.fuzz_xml_payload,
        }

    def fuzz_payload(self, payload, payload_type):
        fuzzer = self.fuzzers.get(payload_type)
        if fuzzer:
            return fuzzer(payload)
        return []

    def fuzz_json_payload(self, payload):
        """Fuzz JSON payload for greater data coverage, returning list of fuzzed versions of the input payload"""
        fuzzes = [payload.replace("{", "").replace("}", ""), payload.replace(
            '"', ''), payload.replace('"', '').replace("{", "").replace("}", "")]
        return fuzzes

    def fuzz_xml_payload(self, payload):
        """Fuzz XML payload for greater data coverage, returning list of fuzzed versions of the input payload"""
        fuzzes = [payload.replace("<", "").replace(">", ""), payload.replace(
            '/', ''), payload.replace('<', '').replace(">", "").replace("/", "")]
        return fuzzes
