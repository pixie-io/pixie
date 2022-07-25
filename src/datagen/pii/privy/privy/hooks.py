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
from privy.providers import Providers


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

    def payload_generator_hook(self):
        # apply to all schemas globally
        @schemathesis.hooks.register
        def before_generate_case(context, strategy):
            """choose data providers for synthetic request based on api parameter name or schema
            i.e. set custom hypothesis strategy to generate data"""
            # APIOperation
            op = context.operation

            def tune_case(case):
                # overwrite default schemathesis strategies (data generators)
                # for every parameter, check if matches pii type in Providers
                for i in range(len(op.path_parameters)):
                    # 1. choose provider using 'name' field of this parameter definition
                    name = str(op.path_parameters.items[i].definition.get('name', None))
                    label, provider = self.providers.get_pii(name)
                    if provider:
                        self.logger.debug(f"{str(name)} |matched this provider| {label}")
                        self.pii_types.append(label)
                        # generate pii value using matched provider for this path parameter
                        case.path_parameters[name] = provider
                        continue
                    # 2. no pii provider matched using 'name', check for non pii keywords
                    nonpii_label, nonpii_provider = self.providers.get_nonpii(name)
                    if nonpii_provider:
                        self.logger.debug(f"{str(name)} |matched this provider| {nonpii_label}")
                        case.path_parameters[name] = nonpii_provider
                return case
            return strategy.map(tune_case)
