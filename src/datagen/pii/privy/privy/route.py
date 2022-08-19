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
import logging
from dicttoxml import dicttoxml
from privy.sql import SQLQueryBuilder
from privy.generate.utils import PrivyFileType


class PayloadRoute:
    def __init__(self, file_writers, analyzer, args):
        self.file_writers = file_writers
        self.conversions = {
            "json": (json.dumps, {"default": "str"}),
            "xml": (dicttoxml, {}),
            "sql": (SQLQueryBuilder(args.region).build_query, {}),
            # todo @benkilimnik protobuf conversion
        }
        self.args = args
        self.analyzer = analyzer
        self.unique_payloads = set()
        self.fuzzer = PayloadFuzzer()

    def is_duplicate(self, case_attr):
        """check if payload template with given arrangement of parameters already exists"""
        payload_params = json.dumps(case_attr, default=str)
        if payload_params in self.unique_payloads:
            logging.getLogger("privy").debug(
                f"Skipping duplicate case: {payload_params}")
            return True
        self.unique_payloads.add(payload_params)

    def write_fuzzed_payloads(self, row, generate_type, writer):
        for fuzzed_payload in self.fuzzer.fuzz_payload(row[0], generate_type):
            row[0] = fuzzed_payload
            writer.csv_writer.writerow(row)

    def write_payload_to_csv(self, payload_template, has_pii, pii_types):
        if not payload_template or "null" in payload_template.values() or self.is_duplicate(payload_template):
            return
        if pii_types:
            self.analyzer.update_pii_counters(pii_types)
        has_pii = str(int(has_pii))
        pii_types = ",".join(set(pii_types))
        for generate_type, privy_writers in self.file_writers.items():
            # convert case template (dict) to other types (json, sql, xml), and then to str for template parsing
            converter, kwargs = self.conversions.get(generate_type, None)
            converted_payload_template = str(
                converter(payload_template, **kwargs))
            for writer in privy_writers:
                if writer.file_type == PrivyFileType.PAYLOADS:
                    # todo @benkilimnik: generate specific instance for this template (next diff)
                    if self.args.fuzz_payloads:
                        self.write_fuzzed_payloads([converted_payload_template, has_pii, pii_types],
                                                   generate_type, writer)
                    writer.csv_writer.writerow(
                        [converted_payload_template, has_pii, pii_types])
                if writer.file_type == PrivyFileType.TEMPLATES:
                    writer.open_file.write(f"{converted_payload_template}\n")
        self.analyzer.update_payload_counts()


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
