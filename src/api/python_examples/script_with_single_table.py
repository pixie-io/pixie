# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import pxapi


# You'll need to generate an API token.
# For more info, see: https://docs.pixielabs.ai/using-pixie/api-quick-start/
API_TOKEN = "<YOUR_API_TOKEN>"

# PxL script with single output table
PXL_SCRIPT = """
import px
df = px.DataFrame('http_events')[['resp_status','req_path']]
df = df.head(10)
px.display(df, 'http_table')
"""

# create a Pixie client
px_client = pxapi.Client(token=API_TOKEN)

# find first healthy cluster
cluster = px_client.list_healthy_clusters()[0]
print("Selected cluster: ", cluster.name())

# connect to first healthy cluster
conn = px_client.connect_to_cluster(cluster)

# execute the PxL script
script = conn.prepare_script(PXL_SCRIPT)

# print results
for row in script.results("http_table"):
    print(row["resp_status"], row["req_path"])
