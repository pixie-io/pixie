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

import time
import logging
import os
import schedule
import pxapi
from slack.web.client import WebClient
from slack.errors import SlackApiError


# Pixie PxL script:
# This script ouputs a table of the HTTP requests count and
# HTTP error (>4xxx) count for each service in the `px-sock-shop` namespace.
PXL_SCRIPT = """
import px

df = px.DataFrame(table='http_events', start_time='-5m')

# Add column for HTTP response status errors.
df.error = df.http_resp_status >= 400

# Add columns for service, namespace info
df.namespace = df.ctx['namespace']
df.service = df.ctx['service']

# Filter for px-sock-shop namespace only.
df = df[df.namespace == 'px-sock-shop']

# Group HTTP events by service, counting errors and total HTTP events.
df = df.groupby(['service']).agg(
    error_count=('error', px.sum),
    total_requests=('http_resp_status', px.count)
)

px.display(df, "status")
"""

logging.basicConfig(level=logging.DEBUG)


# Get data from the Pixie API.
def get_pixie_data(cluster_conn):

    # Execute the PxL script.
    script = cluster_conn.prepare_script(PXL_SCRIPT)
    logging.debug("Pixie cluster executed script.")

    service_stats_msg = ["*Recent 4xx+ Spikes in last 5 minutes:*"]

    # Process table output rows to construct slack message.
    for row in script.results("status"):
        service_stats_msg.append(format_message(row["service"],
                                                row["total_requests"],
                                                row["error_count"]))

    return "\n\n".join(service_stats_msg)


# Format Pixie API table row data.
def format_message(service, request_count, error_count):
    return (f"`{service}` \t ---> {error_count} (>4xx)"
            f" errors out of {request_count} requests.")


# Send Slack message through the Slack client.
def send_message(slack_client, channel, cluster_conn):

    # Get data from the Pixie API.
    msg = get_pixie_data(cluster_conn)

    # Send a POST request through the Slack client.
    try:
        logging.info(f"Sending {msg!r} to {channel!r}")
        slack_client.chat_postMessage(channel=channel, text=msg)

    except SlackApiError as e:
        logging.error('Request to Slack API Failed: {}.'.format(e.response.status_code))
        logging.error(e.response)


if __name__ == "__main__":

    # Get Pixie API key.
    if "PIXIE_API_KEY" not in os.environ:
        logging.error("Missing `PIXIE_API_KEY` environment variable.")
    PIXIE_API_KEY = os.environ['PIXIE_API_KEY']

    # Create a Pixie client.
    logging.debug("Authorizing Pixie client.")
    px_client = pxapi.Client(token=PIXIE_API_KEY)

    # Get Pixie cluster ID.
    if "PIXIE_CLUSTER_ID" not in os.environ:
        logging.error("Missing `PIXIE_CLUSTER_ID` environment variable.")
    PIXIE_CLUSTER_ID = os.environ['PIXIE_CLUSTER_ID']

    # Connect to cluster.
    cluster_conn = px_client.connect_to_cluster(PIXIE_CLUSTER_ID)
    logging.debug("Pixie client connected to %s cluster.", cluster_conn.name())

    # Get Slackbot access token.
    if "SLACK_BOT_TOKEN" not in os.environ:
        logging.error("Missing `SLACK_BOT_TOKEN` environment variable.")
    SLACK_BOT_TOKEN = os.environ['SLACK_BOT_TOKEN']

    # Create a Slack client.
    logging.debug("Authorizing Slack client.")
    slack_client = WebClient(SLACK_BOT_TOKEN)

    # Slack channel to post in. Slack App must be a member of this channel.
    SLACK_CHANNEL = "#pixie-alerts"

    # Schedule sending a Slack channel message every 5 minutes.
    schedule.every(5).minutes.do(lambda: send_message(slack_client,
                                                      SLACK_CHANNEL,
                                                      cluster_conn))

    logging.info("Message scheduled for %s Slack channel.", SLACK_CHANNEL)

    while True:
        schedule.run_pending()

        # Sleep for 5 seconds between checks on the scheduler.
        time.sleep(5)
