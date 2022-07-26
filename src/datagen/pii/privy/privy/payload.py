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
import random
from datetime import timedelta
from copy import deepcopy
import traceback
import schemathesis
from joblib import Parallel, delayed
from tqdm_joblib import tqdm_joblib
from tqdm import tqdm
from alive_progress import alive_bar
from schemathesis import DataGenerationMethod
from hypothesis import (
    HealthCheck,
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

    def __init__(self, folder, csvwriter, generate_type, multi_threaded,
                 insert_pii_percentage, insert_label_pii_percentage):
        self.folder = folder
        self.generate_type = generate_type
        self.logger = logging.getLogger("privy")
        self.route = PayloadRoute(csvwriter, generate_type)
        self.hook = SchemaHooks().schema_analyzer
        self.providers = self.hook.providers
        self.files = []
        self.http_types = ["get", "head", "post", "put",
                           "delete", "connect", "options", "trace", "patch"]
        self.insert_pii_percent = insert_pii_percentage
        self.insert_label_pii_percent = insert_label_pii_percentage
        self.multi_threaded = multi_threaded

    def generate_payloads(self):
        """Generate synthetic API request payloads from openAPI specs."""
        num_files = sum(len(files) for _, _, files in os.walk(self.folder))
        self.logger.info(
            f"Generating synthetic request payloads from {num_files} files in {self.folder}")
        # Retrieve openapi descriptor files
        for dirpath, _, files in os.walk(self.folder):
            descriptors = filter(lambda f: f in [
                                 "openapi.json", "swagger.json", "openapi.yaml", "swagger.yaml"], files)
            for desc in descriptors:
                file = pathlib.Path(dirpath) / desc
                self.files.append(file)
        # multi-threaded
        if self.multi_threaded:
            with tqdm_joblib(tqdm(total=num_files, position=0, leave=True)) as progress_bar:
                Parallel(n_jobs=10, prefer='threads')(
                    delayed(self.parse_openapi_descriptor)(file) for file in self.files
                )
                progress_bar.update()
        # single-threaded
        else:
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

    def generate_pii_case(self, case_attr, parameter_type):
        """insert additional pii data into a request payload or generate new payload"""
        rand = random.random()
        if rand > self.insert_pii_percent:
            # insert additional payload {insert_pii_percent}% of the time
            return
        else:
            # insert additional PII payload
            if random.randint(0, 1):
                # clear existing parameters 50% of the time
                case_attr.clear()
                self.hook.clear_pii_types(parameter_type)
            if random.randint(0, 1):
                # sample just one pii label 50% of the time
                label, pii, category = self.providers.pick_random_region().get_random_pii()
                self.logger.debug(f"|Inserting additional pii type| {label} |with category| {category}")
                case_attr[label] = pii
                self.hook.add_pii_type(parameter_type, label, category)
            else:
                # choose 0 to {insert_label_pii_percent}% of labels
                percent = random.uniform(0, self.insert_label_pii_percent)
                label_pii_tuples = self.providers.pick_random_region().sample_pii(percent)
                for label, pii, category in label_pii_tuples:
                    self.logger.debug(
                        f"Inserting additional pii type: {label}")
                    case_attr[label] = pii
                    self.hook.add_pii_type(parameter_type, label, category)
        # randomize order of parameters
        case_attr = list(case_attr.items())
        random.shuffle(case_attr)
        case_attr = dict(case_attr)
        return case_attr

    @settings(
        verbosity=Verbosity.quiet,
        deadline=timedelta(milliseconds=5000),
        max_examples=1,
        suppress_health_check=(HealthCheck.too_slow,)
    )
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
            self.route.write_payload_to_csv(
                case.query, self.hook.has_pii(ParamType.QUERY), self.hook.get_pii_types(ParamType.QUERY)
            )
            # insert additional pii if this request is identified as containing pii
            # often in pii requests, the parameters are not given pii keywords for security reasons
            # to account for this we insert additional random pii fields in requests we know contain pii
            if self.hook.has_pii(ParamType.PATH) and case.path_parameters:
                pii_path_params = self.generate_pii_case(deepcopy(case.path_parameters), ParamType.PATH)
                self.route.write_payload_to_csv(
                    pii_path_params, self.hook.has_pii(ParamType.PATH), self.hook.get_pii_types(ParamType.PATH)
                )
                self.hook.clear_pii_types(ParamType.PATH)
            if self.hook.has_pii(ParamType.QUERY) and case.query:
                pii_query_params = self.generate_pii_case(deepcopy(case.query), ParamType.QUERY)
                self.route.write_payload_to_csv(
                    pii_query_params, self.hook.has_pii(ParamType.QUERY), self.hook.get_pii_types(ParamType.QUERY)
                )
                self.hook.clear_pii_types(ParamType.QUERY)
