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

import random
from privy.providers.english_us import English_US


class Providers:
    def __init__(self):
        self.regions = [
            English_US(),
        ]

    def pick_random_region(self):
        """Pick a random region/language specific provider to execute a GenericProvider method on"""
        return random.choice(self.regions)

    def __repr__(self):
        return f"Providers.regions - {self.regions}"
