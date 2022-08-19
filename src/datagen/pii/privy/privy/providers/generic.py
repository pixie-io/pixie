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

import dataclasses
import random
from abc import ABC
from typing import Union, Optional, Type, Callable, Set
from decimal import Decimal


@dataclasses.dataclass()
class Provider:
    """Provider holds information related to a specific (non-)PII provider.
    each provider has a name, a set of aliases, and a faker generator."""

    name: str
    aliases: Set[str]
    generator: Callable
    type_: Union[Type[str], Type[int], Type[float], Type[Decimal]] = str

    def __repr__(self):
        return str(vars(self))


class GenericProvider(ABC):
    """Parent class containing common methods shared by region specific providers"""
    def __init__(self):
        pass

    def get_pii_types(self) -> list[str]:
        """Return all pii types in the pii_label_to_provider dict"""
        return [provider.name for provider in self.pii_providers]

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
            if name.lower() == provider.name or name.lower() in provider.aliases:
                return provider

    def get_nonpii_provider(self, name: str) -> Optional[Provider]:
        """Find non-PII provider that matches input name. Returns None if no match is found."""
        if not name:
            return
        for provider in self.nonpii_providers:
            if name.lower() == provider.name or name.lower() in provider.aliases:
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
