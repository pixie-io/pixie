#! /bin/bash

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

if [[ -z "$PIXIE_ROOT" ]]; then
    echo "Error: Need PIXIE_ROOT as an environment variable."
    exit
fi

echo "Running tshark in the background."
tshark -i any -O mysql -w "$PIXIE_ROOT"/src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/raw.pcap -q > /dev/null&
TSHARK_PID=$!

echo "Running mysql_container_bpf_test."
cd "$PIXIE_ROOT" || exit
sudo ./bazel-bin/src/stirling/mysql_container_bpf_test --tracing_mode=true

echo "Dumping captured mysql traffic."
tshark -i any -2 -r "$PIXIE_ROOT"/src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/raw.pcap -R mysql -T json \
> "$PIXIE_ROOT"/src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/tshark.json

kill -9 "$TSHARK_PID"
