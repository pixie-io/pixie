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
import schemathesis
from privy.chosen_providers import Providers


class SchemaHooks:
    """initialize schemathesis hook to replace default data generation strategies for api path parameters
    and track the pii_types present in a given request payload"""

    def __init__(self):
        self.logger = logging.getLogger("privy")
        self.providers = Providers()
        self.pii_types = []
        self.payload_generator_hook()

    def get_pii(self):
        return self.pii_types

    def add_pii(self, pii_type):
        self.pii_types.append(pii_type)

    def has_pii(self):
        return len(self.pii_types) > 0

    def clear_pii(self):
        self.pii_types.clear()

    def lookup_pii_provider(self, name, case):
        """lookup pii provider for a given parameter name and, if a match is found, assign pii"""
        label_pii_tuple = self.providers.pick_random_region().get_pii(name)
        if label_pii_tuple:
            label, pii = label_pii_tuple
            self.logger.debug(f"{name} |matched this pii provider| {label}")
            self.pii_types.append(label)
            # assign generated pii value to this parameter
            case.path_parameters[name] = pii
            return (label, pii)

    def lookup_nonpii_provider(self, name, case):
        """lookup nonpii provider for a given parameter name and, if a match is found, assign generated nonpii"""
        label_nonpii_tuple = self.providers.pick_random_region().get_nonpii(name)
        if label_nonpii_tuple:
            label, nonpii = label_nonpii_tuple
            self.logger.debug(f"{name} |matched this nonpii provider| {label}")
            # assign generated nonpii value to this parameter
            case.path_parameters[name] = nonpii
            return (label, nonpii)

    def payload_generator_hook(self):
        """apply this hook to api schema parsing globally, identifying pii types in the request payloads"""

        @schemathesis.hooks.register
        def before_generate_case(context, strategy):
            """choose data providers for synthetic request based on api parameter name or schema"""
            # APIOperation
            op = context.operation

            def tune_case(case):
                # overwrite default schemathesis strategies (data generators)
                # for every parameter, check if matches pii type in Providers
                for i in range(len(op.path_parameters)):
                    # 1. match provider using 'name' field of this parameter definition
                    name = op.path_parameters[i].definition.get("name", None)
                    if self.lookup_pii_provider(name, case):
                        # pii found, so continue to next path parameter
                        continue
                    # 2. no pii provider matched in 'name', check for non pii keywords
                    if self.lookup_nonpii_provider(name, case):
                        # nonpii found, so continue to next path parameter
                        continue
                return case

            return strategy.map(tune_case)
