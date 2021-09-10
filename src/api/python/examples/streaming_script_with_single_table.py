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
import pxapi


# You'll need to generate an API token.
# For more info, see: https://docs.px.dev/using-pixie/api-quick-start/
API_TOKEN = os.getenv("PX_API_KEY")
CLUSTER_ID = os.getenv("PX_CLUSTER_ID")

# PxL script with single streaming output tables
PXL_SCRIPT = """
import px
df = px.DataFrame('http_events')[['resp_status','req_path']]
px.display(df.stream(), 'http_table')
"""


# create a function to process the table rows
def http_fn(row: pxapi.Row) -> None:
    print(row["resp_status"], row["req_path"])


# create a Pixie client
px_client = pxapi.Client(token=API_TOKEN)
conn = px_client.connect_to_cluster(CLUSTER_ID)

# setup the PxL script
script = conn.prepare_script(PXL_SCRIPT)

# Add a callback function to the script
script.add_callback("http_table", http_fn)

# run the script
script.run()
