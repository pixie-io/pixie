import os
import pxapi


# You'll need to generate an API token.
# For more info, see: https://docs.pixielabs.ai/using-pixie/api-quick-start/
API_TOKEN = os.getenv("PX_API_KEY")
CLUSTER_ID = os.getenv("PX_CLUSTER_ID")

# PxL script with single output table
PXL_SCRIPT = """
import px
df = px.DataFrame('http_events')[['resp_status','req_path']]
df = df.head(10)
px.display(df, 'http_table')
"""

# create a Pixie client
px_client = pxapi.Client(token=API_TOKEN)
conn = px_client.connect_to_cluster(CLUSTER_ID)

# execute the PxL script
script = conn.prepare_script(PXL_SCRIPT)

# print results
for row in script.results("http_table"):
    print(row["resp_status"], row["req_path"])
