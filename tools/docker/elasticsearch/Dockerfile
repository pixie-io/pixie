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

FROM docker.elastic.co/elasticsearch/elasticsearch:7.6.0@sha256:fb37d2e15d897b32bef18fed6050279f68a76d8c4ea54c75e37ecdbe7ca10b4b

RUN zip -q -d /usr/share/elasticsearch/lib/log4j-core-2.11.1.jar org/apache/logging/log4j/core/lookup/JndiLookup.class

CMD ["eswrapper"]
