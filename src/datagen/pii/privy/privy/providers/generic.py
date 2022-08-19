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
import dataclasses
import random
import string
from abc import ABC
from typing import Union, Optional, Type, Callable, Set
from decimal import Decimal
import baluhn
from faker.providers import BaseProvider
from faker.providers.lorem.en_US import Provider as LoremProvider
from privy.providers.spans import PayloadSpans


@dataclasses.dataclass()
class Provider:
    """Provider holds information related to a specific (non-)PII provider.
    each provider has a name, a set of aliases, and a faker generator."""

    template_name: str
    aliases: Set[str]
    generator: Callable
    type_: Union[Type[str], Type[int], Type[float],
                 Type[Decimal], Type[bool]] = str

    def __repr__(self):
        return str(vars(self))


class GenericProvider(ABC):
    """Parent class containing common methods shared by region specific providers"""

    def __init__(self):
        pass

    def get_pii_types(self) -> list[str]:
        """Return all pii types in the pii_label_to_provider dict"""
        return [provider.template_name for provider in self.pii_providers]

    def get_delimited(self, label: str) -> list[str]:
        """Return list of versions of input label with different delimiters"""
        label_delimited = [
            label,
            label.replace(" ", "-"),
            label.replace(" ", "_"),
            label.replace(" ", "__"),
            label.replace(" ", "."),
            label.replace(" ", ":"),
            label.replace(" ", ""),
        ]
        return label_delimited

    def get_pii_provider(self, name: str) -> Optional[Provider]:
        """Find PII provider that matches input name. Returns None if no match is found."""
        if not name:
            return
        for provider in self.pii_providers:
            if name.lower() == provider.template_name or name.lower() in provider.aliases:
                return provider

    def get_nonpii_provider(self, name: str) -> Optional[Provider]:
        """Find non-PII provider that matches input name. Returns None if no match is found."""
        if not name:
            return
        for provider in self.nonpii_providers:
            if name.lower() == provider.template_name or name.lower() in provider.aliases:
                return provider

    def get_random_pii_provider(self) -> Provider:
        """choose random PII provider and generate a value"""
        return random.choice(list(self.pii_providers))

    def sample_pii_providers(self, percent: float) -> list[Provider]:
        """Sample a random percentage of PII providers and associated values"""
        return random.sample(list(self.pii_providers), round(len(self.pii_providers) * percent))

    def filter_providers(self, pii_types: list[str]) -> None:
        """Filter out PII types not in the given list, marking them as non-PII"""
        if not pii_types:
            pii_types = self.get_pii_types()

    def get_faker(self, faker_provider: str):
        faker_generator = getattr(self.f, faker_provider)
        return faker_generator

    def parse(self, template: str, template_id: int) -> PayloadSpans:
        """Parse payload template into a span, using data providers that match the template_names e.g. {{full_name}}"""
        return self.f.parse(template=template, template_id=template_id)

    def add_provider_alias(self, provider: Callable, alias: str) -> None:
        """Add copy of an existing provider, but under a different name"""
        logging.getLogger("privy").debug(f"Adding alias {alias} for provider {provider}")
        new_provider = BaseProvider(self.f)
        setattr(new_provider, alias, provider)
        self.f.add_provider(new_provider)

    def set_provider_aliases(self):
        """Set faker generator aliases for all providers to reduce mismatch between template_names and generators"""
        for pii in self.pii_providers:
            self.add_provider_alias(provider=pii.generator, alias=pii.template_name)
        for nonpii in self.nonpii_providers:
            self.add_provider_alias(provider=nonpii.generator, alias=nonpii.template_name)


class MacAddress(BaseProvider):
    def mac_address(self) -> str:
        pattern = random.choice(
            [
                "^^:^^:^^:^^:^^:^^",
                "^^-^^-^^-^^-^^-^^",
                "^^ ^^ ^^ ^^ ^^ ^^",
            ]
        )
        return self.hexify(pattern)


class IMEI(BaseProvider):
    def imei(self) -> str:
        imei = self.numerify(text="##-######-######-#")
        while baluhn.verify(imei.replace("-", "")) is False:
            imei = self.numerify(text="##-######-######-#")
        return imei


class Gender(BaseProvider):
    def gender(self) -> str:
        return random.choice(["Male", "Female", "Other"])


class Passport(BaseProvider):
    def passport(self) -> str:
        # US Passports consist of 1 letter or digit followed by 8-digits
        return self.bothify(text=random.choice(["?", "#"]) + "########")


class DriversLicense(BaseProvider):
    def drivers_license(self) -> str:
        # US driver's licenses consist of 9 digits (patterns vary by state)
        return self.numerify(text="### ### ###")


class Alphanum(BaseProvider):
    def alphanum(self) -> str:
        alphanumeric_string = "".join(
            [random.choice(["?", "#"])
                for _ in range(random.randint(1, 15))]
        )
        return self.bothify(text=alphanumeric_string)


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
