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

import torch


class MeanPerClass:
    def __init__(self, num_classes):
        self.confusion_matrix = torch.zeros(num_classes, num_classes)

    def update(self, targets, preds):
        for t, p in zip(targets.flatten(), preds.flatten()):
            self.confusion_matrix[t.item(), p.item()] += 1

    def avg(self):
        return torch.mean(
            self.confusion_matrix.diag() / (self.confusion_matrix.sum(1) + 1e-8)).item()

    def __str__(self):
        return "MeanPerClass"
