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


logging.basicConfig(level=logging.INFO)

# The slackbot requires the following configs, which are specified
# using environment variables. For directions on how to find these
# config values, see: https://docs.pixielabs.ai/tutorials/slackbot-alert
if "PIXIE_API_KEY" not in os.environ:
    logging.error("Missing `PIXIE_API_KEY` environment variable.")
pixie_api_key = os.environ['PIXIE_API_KEY']

if "PIXIE_CLUSTER_ID" not in os.environ:
    logging.error("Missing `PIXIE_CLUSTER_ID` environment variable.")
pixie_cluster_id = os.environ['PIXIE_CLUSTER_ID']

if "SLACK_BOT_TOKEN" not in os.environ:
    logging.error("Missing `SLACK_BOT_TOKEN` environment variable.")
slack_bot_token = os.environ['SLACK_BOT_TOKEN']

# This PxL script ouputs a table of the HTTP total requests count and
# HTTP error (>4xxx) count for each service in the `px-sock-shop` namespace.
# To deploy the px-sock-shop demo, see:
# https://docs.pixielabs.ai/tutorials/slackbot-alert for how to
pxl_script = open("http_errors.pxl", "r").read()


def get_pixie_data(cluster_conn):

    msg = ["*Recent 4xx+ Spikes in last 5 minutes:*"]

    script = cluster_conn.prepare_script(pxl_script)

    # If you change the PxL script, you'll need to change the
    # columns this script looks for in the result table.
    for row in script.results("http_table"):
        msg.append(format_message(row["service"],
                                  row["total_requests"],
                                  row["error_count"]))

    return "\n\n".join(msg)


def format_message(service, request_count, error_count):
    return (f"`{service}` \t ---> {error_count} (>4xx)"
            f" errors out of {request_count} requests.")


def send_slack_message(slack_client, channel, cluster_conn):

    # Get data from the Pixie API.
    msg = get_pixie_data(cluster_conn)

    # Send a POST request through the Slack client.
    try:
        logging.info(f"Sending {msg!r} to {channel!r}")
        slack_client.chat_postMessage(channel=channel, text=msg)

    except SlackApiError as e:
        logging.error('Request to Slack API Failed: {}.'.format(e.response.status_code))
        logging.error(e.response)


def main():

    logging.debug("Authorizing Pixie client.")
    px_client = pxapi.Client(token=pixie_api_key)

    cluster_conn = px_client.connect_to_cluster(pixie_cluster_id)
    logging.debug("Pixie client connected to %s cluster.", cluster_conn.name())

    logging.debug("Authorizing Slack client.")
    slack_client = WebClient(slack_bot_token)

    # Slack channel for Slackbot to post in.
    # Slack App must be a member of this channel.
    SLACK_CHANNEL = "#pixie-alerts"

    # Schedule sending a Slack channel message every 5 minutes.
    schedule.every(5).minutes.do(lambda: send_slack_message(slack_client,
                                                            SLACK_CHANNEL,
                                                            cluster_conn))

    logging.info("Message scheduled for %s Slack channel.", SLACK_CHANNEL)

    while True:
        schedule.run_pending()
        time.sleep(5)


if __name__ == "__main__":
    main()
