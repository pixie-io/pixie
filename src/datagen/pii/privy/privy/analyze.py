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

import logging
from collections import Counter
from typing import Tuple

from privy.providers.generic import GenericProvider


class DatasetAnalyzer:
    def __init__(self, providers: GenericProvider) -> None:
        # initialize counter of PII types in the generated dataset
        # initialize all pii type counts to 0
        self.count_pii_types = Counter(
            {k: 0 for k in providers.get_pii_types()})
        self.num_payloads = 0
        self.num_pii_payloads = 0
        self.num_payloads_this_spec = 0
        self.num_pii_payloads_this_spec = 0
        self.percent_pii = 0
        self.num_pii_types_per_pii_payload = 0
        self.log = logging.getLogger("privy")

    def get_lowest_count_pii_type(self) -> Tuple[str, int]:
        """Get pii type with lowest count in the generated dataset"""
        return min(self.count_pii_types.items(), key=lambda x: x[1])

    def k_lowest_pii_types(self, k) -> list[Tuple[str, int]]:
        """Get k pii types with lowest count in the generated dataset"""
        return self.count_pii_types.most_common()[:-k - 1:-1]

    def update_pii_counters(self, pii_types) -> None:
        self.count_pii_types.update(pii_types)
        self.num_pii_payloads += 1
        self.num_pii_payloads_this_spec += 1

    def reset_spec_specific_metrics(self) -> None:
        self.num_payloads_this_spec = 0
        self.num_pii_payloads_this_spec = 0

    def update_payload_counts(self) -> None:
        self.num_payloads += 1
        self.num_payloads_this_spec += 1
        if self.num_pii_payloads > 0:
            self.percent_pii = (self.num_pii_payloads / self.num_payloads) * 100
            num_pii_types = sum(self.count_pii_types.values())
            self.num_pii_types_per_pii_payload = num_pii_types / self.num_pii_payloads

    def print_metrics(self) -> None:
        self.log.info(
            f"Dataset has these pii_type counts: {self.count_pii_types}")
        self.log.info(
            f"{self.num_pii_payloads_this_spec} out of {self.num_payloads_this_spec} \
            generated payloads contain PII for this api spec")
        self.log.info(
            f"{self.percent_pii:.2f}% of payloads contain PII")
        self.log.info(
            f"Each PII payload has {self.num_pii_types_per_pii_payload:.2f} PII types on average")
