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

from model.metadata import kTargetProtocols


class MisclassifiedExampleGenerator:
    """
    Given the predictions and targets of a batch, generate examples of misclassified
    packets/connections.
    """
    def __init__(self, output_file, k=10):
        """
        :param output_file: A csv file. Each row contains payload, prediction, and target.
        :param k: The number of misclassified examples from protocol i to j to output.
        """
        self.f = open(output_file, "w")
        self.f.write("payload\tprediction\ttarget\n")

        self.idx2protocol = {i: protocol for i, protocol in enumerate(kTargetProtocols)}
        self.k = k

        self.counts = {}
        for i in range(len(kTargetProtocols)):
            for j in range(len(kTargetProtocols)):
                self.counts[(i, j)] = 0

    def update(self, payloads, targets, preds):
        for payload, pred, target in zip(payloads, preds, targets):
            if pred == target:
                continue

            pred_idx, target_idx = pred.item(), target.item()

            if self.counts[(pred_idx, target_idx)] < self.k:
                self.counts[(pred_idx, target_idx)] += 1

                if type(payload) == list:
                    payload_str = ",".join([p.hex() for p in payload])
                else:
                    payload_str = payload.hex()

                row = "\t".join([payload_str, self.idx2protocol[pred_idx],
                                 self.idx2protocol[target_idx]]) + "\n"
                self.f.write(row)

    def close(self):
        self.f.close()
