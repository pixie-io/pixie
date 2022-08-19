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
import string
import random
from privy.providers.english_us import English_US
from faker.providers import BaseProvider
from faker.providers.lorem.de_DE import Provider as LoremProvider


# override gender provider from English_US
class Gender(BaseProvider):
    def gender(self):
        return random.choice(["Männlich", "Weiblich", "Sonstige"])


class Passport(BaseProvider):
    # override us_passport from English_US
    def passport(self):
        # German Passports consist of 27 characters and digits
        # (excluding a, e, i, o, u, ae, oe, ue, b, s, q, d)
        allowed_chars = [c for c in string.ascii_uppercase + string.digits if c not in "aeioubsqd"]
        return "".join(random.sample(allowed_chars, 27))


class DriversLicense(BaseProvider):
    # override us_drivers_license from English_US
    def drivers_license(self):
        # German driver's licenses consist of 4 digits followed by 7 alphanumeric chars
        lic = "".join(random.sample(string.digits, 4))
        return lic.join(random.sample(string.ascii_uppercase + string.digits, 7))


class String(LoremProvider):
    def string(self) -> str:
        """generate a random string of characters, words, and numbers"""
        def sample(text, low, high, space=False):
            """sample randomly from input text with a minimum length of low and maximum length of high"""
            space = " " if space else ""
            return space.join(random.sample(text, random.randint(low, high)))

        characters = sample(string.ascii_letters, 1, 10)
        numbers = sample(string.digits, 1, 10)
        characters_and_numbers = sample(
            string.ascii_letters + string.digits, 1, 10)
        combined = self.words(
            nb=3) + [characters, numbers, characters_and_numbers]
        return sample(combined, 0, 6, True)


# German Germany - inherits methods from English_US
class German_DE(English_US):
    def __init__(self, pii_types=None):
        # initialize English_US methods and providers, changing faker locale to de_DE
        super().__init__(pii_types, locale="de_DE")
        self.f.add_provider(Gender)
        self.f.add_provider(Passport)
        self.f.add_provider(DriversLicense)
        self.f.add_provider(String)
