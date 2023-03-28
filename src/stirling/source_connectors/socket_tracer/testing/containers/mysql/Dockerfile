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

FROM python:3.10.0-alpine3.14@sha256:bed0a5fe5b7d996b0678abf4fb2f0d5f3d5078cda951ccdc068ffbddb555e727

RUN pip install --trusted-host pypi.python.org -q mysql-connector-python

# A location for storing scripts.
RUN mkdir /scripts

# Give python a startup script that prints the python interpreter pid.
# Used by the tests to filter by pid when ensuring that the MySQL messages were traced.
RUN mkdir -p /root/.local/lib/python3.10/site-packages
RUN echo "import os" > /root/.local/lib/python3.10/site-packages/sitecustomize.py
RUN echo "print(\"pid=\" + str(os.getpid()))" >> /root/.local/lib/python3.10/site-packages/sitecustomize.py

# Entrypoint is python. Accepts a script as an argument.
ENTRYPOINT ["python"]
