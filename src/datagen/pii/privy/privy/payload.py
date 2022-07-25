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
import os
import pathlib
import warnings
import logging
from datetime import timedelta
import traceback
import schemathesis
from alive_progress import alive_bar
from schemathesis import DataGenerationMethod
from hypothesis import (
    given,
    Verbosity,
    settings,
    strategies as st,
)
from privy.hooks import SchemaHooks, ParamType
from privy.route import PayloadRoute
# todo @benkilimnik add fine grained warning filter for schemathesis
warnings.filterwarnings("ignore")


class PayloadGenerator:

    def __init__(self, folder, csvwriter, generate_type):
        self.folder = folder
        self.generate_type = generate_type
        self.logger = logging.getLogger("privy")
        self.route = PayloadRoute(csvwriter, generate_type)
        self.hook = SchemaHooks()
        self.files = []
        self.http_types = ["get", "head", "post", "put", "delete", "connect", "options", "trace", "patch"]

    def generate_payloads(self):
        """Generate synthetic API request payloads from openAPI specs."""
        num_files = sum(len(files) for _, _, files in os.walk(self.folder))
        self.logger.info(f"Generating synthetic request payloads from {num_files} files in {self.folder}")
        # Retrieve openapi descriptor files
        for dirpath, _, files in os.walk(self.folder):
            descriptors = filter(lambda f: f in ["openapi.json", "swagger.json", "openapi.yaml", "swagger.yaml"], files)
            for desc in descriptors:
                file = pathlib.Path(dirpath) / desc
                self.files.append(file)

        with alive_bar(num_files) as progress_bar:
            for file in self.files:
                self.parse_openapi_descriptor(file)
                progress_bar()

    def parse_openapi_descriptor(self, file):
        self.logger.debug(f"Generating {file}...")
        try:
            schema = schemathesis.from_path(
                file, data_generation_methods=[DataGenerationMethod.positive]
            )
            self.parse_http_methods(schema)
            self.logger.debug("Success")
        except Exception:
            self.logger.debug(traceback.format_exc())

    @settings(verbosity=Verbosity.quiet, deadline=timedelta(milliseconds=5000), max_examples=1)
    @given(data=st.data())
    def parse_http_methods(self, data, schema):
        """instantiate synthetic request payload and choose data providers for a given openapi spec"""
        for path in schema.keys():
            for http_type in self.http_types:
                method = schema[path].get(http_type, None)
                if method:
                    break
            # choose default strategies (data generators in hypothesis) based on schema of api path
            strategy = method.as_strategy()
            # replace default strategies with custom providers in providers.py using before_generate_case in hooks.py
            # and generate data for path parameters in this api spec
            case = data.draw(strategy)
            # write generated request parameters to csv
            self.route.write_payload_to_csv(
                case.path_parameters, self.hook.has_pii(ParamType.PATH), self.hook.get_pii_types(ParamType.PATH)
            )
            self.hook.clear_pii_types(ParamType.PATH)
            self.route.write_payload_to_csv(
                case.query, self.hook.has_pii(ParamType.QUERY), self.hook.get_pii_types(ParamType.QUERY)
            )
            self.hook.clear_pii_types(ParamType.QUERY)
