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

from abc import ABC, abstractmethod


class GenericProvider(ABC):
    """Parent class containing common method stubs shared by region specific providers"""
    def __init__(self):
        pass

    @abstractmethod
    def get_pii(self, name):
        """Find label that matches input name, and return generated pii value using matched provider
        Returns None if no match is found."""
        pass

    @abstractmethod
    def get_nonpii(self, name):
        """Find label that matches input name, and return generated pii value using matched provider.
        Returns None if no match is found."""
        pass

    @abstractmethod
    def get_random_pii(self):
        """choose random label and generate a pii value"""
        pass

    @abstractmethod
    def sample_pii(self, percent):
        """Sample a random percentage of pii labels and associated pii values"""
        pass
