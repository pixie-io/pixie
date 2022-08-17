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
from abc import ABC
from collections import namedtuple


class GenericProvider(ABC):
    """Parent class containing common methods shared by region specific providers"""
    def __init__(self):
        # initialize named tuple to hold matched pii provider data for a given pii label
        self.PII = namedtuple("PII", ["category", "label", "value"])
        self.NonPII = namedtuple("NonPII", ["label", "value"])

    def get_pii_categories(self):
        """Return list of pii categories"""
        return self.pii_label_to_provider.keys()

    def get_category(self, category):
        """Return list of pii labels in a category"""
        return self.pii_label_to_provider[category]

    def get_delimited(self, label):
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

    def get_pii(self, name):
        """Find label that matches input name, and return generated pii value using matched provider
        Returns None if no match is found."""
        if not name:
            return
        for category in self.get_pii_categories():
            for label, provider in self.pii_label_to_provider[category].items():
                # check if name at least partially matches pii label
                # for multiword labels, check versions of the label with different delimiters
                label_delimited = self.get_delimited(label)
                for lbl in label_delimited:
                    if lbl.lower() == name.lower():
                        return self.PII(category, label, str(provider()))

    def get_nonpii(self, name):
        """Find label that matches input name, and return generated pii value using matched provider.
        Returns None if no match is found."""
        if not name:
            return
        for label, provider in self.nonpii_label_to_provider.items():
            # check if name at least partially matches nonpii label
            # for multiword labels, check versions of the label with different delimiters
            label_delimited = self.get_delimited(label)
            for lbl in label_delimited:
                if lbl.lower() == name.lower():
                    return self.NonPII(label, str(provider()))

    def get_random_pii(self):
        """choose random label and generate a pii value"""
        category = random.choice(list(self.get_pii_categories()))
        label = random.choice(
            list(self.get_category(category).keys()))
        return self.get_pii(label)

    def sample_pii(self, percent):
        """Sample a random percentage of pii labels and associated pii values"""
        # randomly select a category
        category = random.choice(list(self.get_pii_categories()))
        labels = random.sample(
            list(self.get_category(category).keys()),
            round(
                len(self.get_category(category).keys()) * percent),
        )
        return [self.get_pii(label) for label in labels]
