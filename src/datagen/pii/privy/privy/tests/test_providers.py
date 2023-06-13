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

from hypothesis import given
from hypothesis import strategies as st

from privy.providers.english_us import English_US
from privy.providers.german_de import German_DE


class TestProviders(unittest.TestCase):
    def setUp(self):
        self.provider_regions = [English_US(), German_DE()]

    def test_get_pii_provider(self):
        def eval_provider(provider):
            value = getattr(region.custom_faker, provider.template_name)()
            self.assertTrue(
                isinstance(value, provider.type_),
                f"Provider {provider.template_name} should generate {provider.type_}, not {type(value)}",
            )

        for region in self.provider_regions:
            for provider in region.pii_providers:
                eval_provider(provider)
            for provider in region.nonpii_providers:
                eval_provider(provider)

    def test_get_random_pii(self):
        for region in self.provider_regions:
            random_provider = region.get_random_pii_provider()
            pii_types = region.get_pii_types()
            self.assertTrue(random_provider.template_name in pii_types)

    @given(decimal=st.decimals(min_value=0, max_value=1))
    def test_sample_pii_providers(self, decimal):
        for region in self.provider_regions:
            sampled_providers = region.sample_pii_providers(decimal)
            num_samples = len(sampled_providers)
            pii_types = region.get_pii_types()
            self.assertTrue(
                # check that number of samples matches given percentage
                num_samples == round(len(pii_types) * decimal)
            )
            self.assertTrue(
                # check that sampled providers are present in pii_types
                all([provider.template_name in pii_types for provider in sampled_providers])
            )


if __name__ == "__main__":
    unittest.main()
