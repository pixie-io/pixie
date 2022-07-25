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

import unittest
from hypothesis import strategies as st, given
from privy.providers import Providers


class TestProviders(unittest.TestCase):
    def setUp(self):
        self.providers = Providers()

    def test_get_pii(self):
        for label in self.providers.pii_label_to_provider.keys():
            label, provider = self.providers.get_pii(label)
            self.assertTrue(
                isinstance(provider, str),
                f"Provider {label} should be str, not {type(provider)}",
            )

    def test_get_nonpii(self):
        for label in self.providers.nonpii_label_to_provider.keys():
            label, provider = self.providers.get_nonpii(label)
            self.assertTrue(
                isinstance(provider, str),
                f"Provider {label} should be str, not {type(provider)}",
            )

    def test_get_random_pii(self):
        self.assertTrue(
            self.providers.get_random_pii()[0] in self.providers.pii_label_to_provider.keys()
        )

    @given(decimal=st.decimals(min_value=0, max_value=1))
    def test_sample_pii_labels(self, decimal):
        self.assertTrue(
            0 <= len(self.providers.sample_pii(decimal)) <= len(self.providers.pii_label_to_provider)
        )
        self.assertEqual(
            len(self.providers.sample_pii(decimal)),
            round(len(self.providers.pii_label_to_provider.keys()) * decimal),
        )

    def test_custom_providers(self):
        alphanumeric_check = [c.isalnum() for c in self.providers.get_nonpii("alphanumeric")]
        self.assertTrue(
            all(alphanumeric_check)
        )
        string_check = [c.isalpha() for c in self.providers.get_nonpii("string")]
        self.assertTrue(
            all(string_check)
        )


if __name__ == "__main__":
    unittest.main()
