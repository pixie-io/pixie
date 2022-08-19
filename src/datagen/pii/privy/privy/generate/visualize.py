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

import csv
import argparse
import sys
from collections import Counter
import pandas as pd
from plotly.subplots import make_subplots
import plotly.graph_objects as go


def parse_args():
    """Perform command-line argument parsing."""

    parser = argparse.ArgumentParser(
        description="Privy data visualizer",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--input",
        "-i",
        required=True,
        help="Absolute path to input data.",
    )
    return parser.parse_args()


def visualize(input_csv):
    csv.field_size_limit(sys.maxsize)
    df = pd.read_csv(input_csv, engine="python", quotechar="|", header=None)
    df.rename(columns={0: "payload", 1: "has_pii", 2: "pii_types", 3: "categories"}, inplace=True)

    # count pii types
    count_pii_types = Counter()
    for pii_types in df["pii_types"]:
        if not pd.isna(pii_types):
            # split pii_types on comma
            count_pii_types.update(pii_types.split(","))
    print(count_pii_types)

    # count categories
    count_categories = Counter()
    for categories in df["categories"]:
        if not pd.isna(categories):
            # split pii_types on comma
            count_categories.update(categories.split(","))
    print(count_categories)

    # compute percentage of payloads that have PII
    num_rows = df.shape[0]
    num_has_pii = df.groupby(['has_pii']).count()['payload'][1]
    percent_pii = round((num_has_pii / num_rows) * 100, 2)
    print(f"payloads (rows) that have PII: {percent_pii}%")

    # count pii types per payload
    avg_num_pii_types_per_payload = round(
        sum(count_pii_types.values()) / num_has_pii, 2)
    print(
        f"Average number of pii types per payload that contains PII: {avg_num_pii_types_per_payload}"
    )

    # graph counts of pii types and categories
    pii_types_df = pd.DataFrame(sorted(count_pii_types.items(
    ), key=lambda item: -item[1]), columns=['PII_type', 'examples_in_dataset'])
    categories_df = pd.DataFrame(sorted(count_categories.items(
    ), key=lambda item: -item[1]), columns=['PII_category', 'examples_in_dataset'])

    fig = make_subplots(y_title="Distribution of PII in Synthetic Protocol Traces", rows=2, cols=1)
    fig.add_trace(go.Bar(name="PII type examples in dataset",
                  x=pii_types_df.PII_type, y=pii_types_df.examples_in_dataset), row=1, col=1)
    fig.add_trace(go.Bar(name="PII category examples in dataset",
                  x=categories_df.PII_category, y=categories_df.examples_in_dataset), row=2, col=1)
    fig.show()


def main(args):
    visualize(args.input)


if __name__ == '__main__':
    args = parse_args()
    main(args)
