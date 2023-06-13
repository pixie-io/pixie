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

import ast
import logging
import random
from copy import deepcopy
from enum import Enum
from typing import Optional, Tuple, Union

import schemathesis
from hypothesis import given
from hypothesis import strategies as st


class ParamType(Enum):
    """Enum for the different types of http parameters that can be generated.
    Used to keep track of the pii types generated for each payload."""
    PATH = 1
    QUERY = 2
    HEADER = 3
    COOKIE = 4


class SchemaHooks:
    """initialize schemathesis hook to replace default data generation strategies for api path parameters
    and track the pii_types present in a given request payload"""

    def __init__(self, args):
        self.schema_analyzer = self.SchemaAnalyzer(args)
        self.payload_generator_hook()

    def payload_generator_hook(self) -> None:
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
                    name = path_param.definition.get("name", None)
                    enum = path_param.definition.get("enum", None)
                    schema = path_param.definition.get("schema", None)
                    type_ = path_param.definition.get("type", None)
                    self.schema_analyzer.assign_parameters(
                        name, enum, schema, type_, case.path_parameters, ParamType.PATH
                    )
                # -------- QUERY PARAMETERS --------
                for query_param in op.query:
                    name = query_param.definition.get("name", None)
                    enum = query_param.definition.get("enum", None)
                    schema = query_param.definition.get("schema", None)
                    type_ = query_param.definition.get("type", None)
                    self.schema_analyzer.assign_parameters(
                        name, enum, schema, type_, case.query, ParamType.QUERY
                    )
                # todo @benkilimnik: generate cookies, headers
                # todo @benkilimnik: parse multi-component schemas that have property params for each component
                return case

            return strategy.map(tune_case)

    class SchemaAnalyzer:
        """Analyze an openapi schema and assign pii values to the corresponding parameter_name in the case_attr"""

        def __init__(self, args):
            self.pii_types = {
                ParamType.PATH: set(),
                ParamType.QUERY: set(),
                ParamType.HEADER: set(),
                ParamType.COOKIE: set(),
            }
            self.providers = args.region
            self.log = logging.getLogger("privy")

        def deepcopy_pii_types(self, parameter_type: ParamType) -> list[str]:
            """deepcopy pii_types list for a given parameter_type"""
            return deepcopy(self.pii_types[parameter_type])

        def overwrite_pii_types(self, parameter_type: ParamType, pii_types: list[str]) -> None:
            """overwrite pii_types list for a given parameter_type"""
            self.pii_types[parameter_type] = pii_types

        def has_pii(self, parameter_type: ParamType) -> bool:
            return len(self.pii_types[parameter_type]) > 0

        def get_pii_types(self, parameter_type: ParamType) -> list[str]:
            return self.pii_types[parameter_type]

        def add_pii_type(self, parameter_type: ParamType, pii_type: str) -> None:
            self.pii_types[parameter_type].add(pii_type)

        def clear_pii_types(self, parameter_type: ParamType) -> None:
            self.pii_types[parameter_type].clear()

        def assign_parameters(self, name: str, enum: Optional[Union[str, list]], schema: Optional[dict],
                              type_: Optional[Union[str, bool]], case_attr: dict, parameter_type: ParamType):
            """assign a provider to a given parameter_name in the case_attr"""
            # Check for enum
            if self.check_for_enum(name, enum, schema, case_attr):
                return case_attr[name]
            # Check for pii keywords
            if self.check_for_pii_keywords(
                name, schema, type_, case_attr, parameter_type
            ):
                return case_attr[name]
            # Check schema for pattern keyword and use provided regex if present
            if self.check_for_regex_pattern(name, schema, case_attr):
                return case_attr[name]
            # Check for non-pii keywords
            if self.check_for_nonpii_keywords(name, schema, type_, case_attr):
                return case_attr[name]
            # last resort, assign string value
            if name:
                self.log.debug(
                    f"{name} |could not be matched. Assigning string...| ")
                case_attr[name] = "{{string}}"

        def check_for_pii_keywords(self, name: str, schema: Optional[dict], type_: Optional[Union[str, bool]],
                                   case_attr: dict, parameter_type: ParamType) -> Optional[bool]:
            """check if a given parameter name or schema contains a pii keyword and, if so, assign pii"""
            if name and self.lookup_pii_provider(
                keyword=name,
                parameter_name=name,
                case_attr=case_attr,
                parameter_type=parameter_type,
            ):
                return True
            if schema:
                if isinstance(schema, str):
                    if self.lookup_pii_provider(
                        keyword=schema,
                        parameter_name=name,
                        case_attr=case_attr,
                        parameter_type=parameter_type,
                    ):
                        return True
                if isinstance(schema, dict):
                    for schema_val in schema.values():
                        if isinstance(schema_val, str) and self.lookup_pii_provider(
                            keyword=schema_val,
                            parameter_name=name,
                            case_attr=case_attr,
                            parameter_type=parameter_type,
                        ):
                            return True
            if type_:
                if isinstance(type_, str):
                    if self.lookup_pii_provider(
                        keyword=type_,
                        parameter_name=name,
                        case_attr=case_attr,
                        parameter_type=parameter_type,
                    ):
                        return True
                if isinstance(type_, bool):
                    case_attr[name] = random.choice(["True", "False"])
                    return True

        def check_for_nonpii_keywords(self, name: str, schema: Optional[dict],
                                      type_: Optional[Union[str, bool]], case_attr: dict) -> Optional[bool]:
            """check if a given parameter name or schema contains a non-pii keyword and, if so, assign pii"""
            if name and self.lookup_nonpii_provider(keyword=name, parameter_name=name, case_attr=case_attr):
                return True
            if schema:
                if isinstance(schema, str):
                    if self.lookup_nonpii_provider(
                        keyword=schema, parameter_name=name, case_attr=case_attr
                    ):
                        return True
                if isinstance(schema, dict):
                    for schema_val in schema.values():
                        if isinstance(schema_val, str) and self.lookup_nonpii_provider(
                            keyword=schema_val, parameter_name=name, case_attr=case_attr
                        ):
                            return True
            if type_:
                if isinstance(type_, str):
                    if self.lookup_nonpii_provider(
                        keyword=type_, parameter_name=name, case_attr=case_attr
                    ):
                        return True
                if isinstance(type_, bool):
                    case_attr[name] = random.choice(["True", "False"])
                    return True

        def lookup_pii_provider(self, keyword: str, parameter_name: str, case_attr: dict,
                                parameter_type: ParamType) -> Optional[Tuple[str, str]]:
            """lookup pii provider for a given keyword and, if a match is found, assign pii for this parameter_name
            and add this pii type to the pii_types list for this parameter_type"""
            pii = self.providers.get_pii_provider(keyword)
            if pii:
                self.log.debug(
                    f"{parameter_name} |matched this pii provider| {pii.template_name}"
                )
                # assign string "{{template_name}}"" of matched PII provider to this parameter
                case_attr[parameter_name] = f"{{{{{pii.template_name}}}}}"
                self.add_pii_type(parameter_type, pii.template_name)
                return (parameter_name, pii.template_name)

        def lookup_nonpii_provider(self, keyword: str, parameter_name: str,
                                   case_attr: dict) -> Optional[Tuple[str, str]]:
            """lookup nonpii provider for a given keyword and, if a match is found,
            assign nonpii for this parameter_name"""
            nonpii = self.providers.get_nonpii_provider(keyword)
            if nonpii:
                self.log.debug(
                    f"{parameter_name} |matched this nonpii provider| {nonpii.template_name}"
                )
                # assign generated nonpii value to this parameter
                case_attr[parameter_name] = f"{{{{{nonpii.template_name}}}}}"
                return (parameter_name, nonpii.template_name)

        def check_for_regex_pattern(self, name: str, schema: Optional[dict], case_attr: dict) -> Optional[bool]:
            """check if a given parameter name or schema contains a regex pattern and, if so, assign pii"""
            if name and schema:
                for schema_key, schema_val in schema.items():
                    if schema_key == "pattern":
                        self.generate_value_from_regex(
                            parameter_name=name, regex=schema_val, case_attr=case_attr
                        )
                        return True

        @given(data=st.data())
        def generate_value_from_regex(self, parameter_name: str, regex: str, case_attr: dict, data) -> None:
            """generate a value from a regex pattern"""
            regex_value = data.draw(st.from_regex(regex, fullmatch=True))
            self.log.debug(
                f"{parameter_name} |matched regex| {regex} |generated| {regex_value}"
            )
            case_attr[parameter_name] = regex_value

        def check_for_enum(self, name: str, enum: Optional[Union[str, list]], schema: Optional[dict],
                           case_attr: dict) -> Optional[bool]:
            """check if a given parameter name or schema contains an enum and, if so, assign pii"""
            if enum:
                self.generate_value_from_enum(
                    parameter_name=name, enum=enum, case_attr=case_attr)
                return True
            if name and schema:
                for schema_key, schema_val in schema.items():
                    if schema_key == "enum":
                        self.generate_value_from_enum(
                            parameter_name=name, enum=schema_val, case_attr=case_attr
                        )
                        return True

        def generate_value_from_enum(self, parameter_name: str, enum: Optional[Union[str, list]],
                                     case_attr: dict) -> None:
            """generate a random value from an enum"""
            # parse string into python list and select random value
            if isinstance(enum, str):
                enum = ast.literal_eval(enum)
            if isinstance(enum, list):
                enum_value = random.choice(enum)
                self.log.debug(
                    f"{parameter_name} |matched enum| {enum} |generated| {enum_value}"
                )
                case_attr[parameter_name] = enum_value
