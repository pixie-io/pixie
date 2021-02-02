import pxapi


# You'll need to generate an API token.
# For more info, see: https://docs.pixielabs.ai/using-pixie/api-quick-start/
API_TOKEN = "<YOUR_API_TOKEN>"

# PxL script with single output table
PXL_SCRIPT = """
import px
df = px.DataFrame('http_events')[['http_resp_status','http_req_path']]
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
    print(row["http_resp_status"], row["http_req_path"])
