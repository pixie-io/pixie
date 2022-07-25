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

    def has_pii(self):
        return len(self.pii_types) > 0

    def get_pii_types(self):
        return self.pii_types

    def add_pii_type(self, pii_type):
        self.pii_types.append(pii_type)

    def clear_pii_types(self):
        self.pii_types.clear()

    def lookup_pii_provider(self, name, case_attr):
        """lookup pii provider for a given parameter name and, if a match is found, assign pii"""
        label_pii_tuple = self.providers.pick_random_region().get_pii(name)
        if label_pii_tuple:
            label, pii = label_pii_tuple
            self.logger.debug(f"{name} |matched this pii provider| {label}")
            # assign generated pii value to this parameter
            case_attr[name] = pii
            self.add_pii_type(label)
            return (label, pii)

    def lookup_nonpii_provider(self, name, case_attr):
        """lookup nonpii provider for a given parameter name and, if a match is found, assign generated nonpii"""
        label_nonpii_tuple = self.providers.pick_random_region().get_nonpii(name)
        if label_nonpii_tuple:
            label, nonpii = label_nonpii_tuple
            self.logger.debug(f"{name} |matched this nonpii provider| {label}")
            # assign generated nonpii value to this parameter
            case_attr[name] = nonpii
            return (label, nonpii)

    def iterate_schema(self, schema, func, *args):
        """iterate over schema and call func on each value, passing case attribute/parameter. If func returns a non-None
        value, break signaling that this case has been dealt with, so no longer need to look through schema for this
        api_method parameter. e.g. for attr=query, case_attr = case.query"""
        for val in schema.values():
            if func(str(val), *args):
                return True

    def check_for_pii_keywords(self, name, schema, case_attr):
        """check if a given parameter name or schema contains a pii keyword and, if so, assign pii"""
        if name and self.lookup_pii_provider(name, case_attr):
            return True
        if schema and self.iterate_schema(schema, self.lookup_pii_provider, case_attr):
            return True

    def check_for_nonpii_keywords(self, name, schema, case_attr):
        """check if a given parameter name or schema contains a non-pii keyword and, if so, assign pii"""
        if name and self.lookup_nonpii_provider(name, case_attr):
            return True
        if schema and self.iterate_schema(schema, self.lookup_nonpii_provider, case_attr):
            return True

    def payload_generator_hook(self):
        """apply this hook to api schema parsing globally, identifying pii types in the request payloads"""
        @schemathesis.hooks.register
        def before_generate_case(context, strategy):
            """choose data providers for synthetic request based on api parameter name or schema"""
            # APIOperation
            op = context.operation

            # overwrite default schemathesis strategies (data generators)
            def tune_case(case):
                # -------- PATH PARAMETERS --------
                for path_param in op.path_parameters:
                    # Check for pii keywords
                    name = path_param.definition.get('name', None)
                    schema = path_param.definition.get('schema', None)
                    if self.check_for_pii_keywords(name, schema, case.path_parameters):
                        continue
                    # Check for non-pii keywords
                    if self.check_for_nonpii_keywords(name, schema, case.path_parameters):
                        continue
                return case
            return strategy.map(tune_case)
