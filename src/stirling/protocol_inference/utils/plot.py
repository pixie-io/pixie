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

import numpy as np
import matplotlib.pyplot as plt
from model.metadata import kTargetProtocols


def plot_confusion_matrix(matrix, save_dir):
    num_protocols = len(kTargetProtocols)

    total_packets = matrix.sum(axis=1)

    # Normalize each row of the matrix.
    matrix = matrix / (total_packets + 1e-8)[:, None]

    fig, ax = plt.subplots()
    ax.imshow(matrix)
    ax.set_xticks(np.arange(num_protocols))
    ax.set_yticks(np.arange(num_protocols))

    ax.set_xticklabels(kTargetProtocols)

    ylabels = [f"{proto}({int(num)})" for proto, num in zip(kTargetProtocols, total_packets)]
    ax.set_yticklabels(ylabels)

    plt.setp(ax.get_xticklabels(), rotation=45, ha="right", rotation_mode="anchor")

    for i in range(num_protocols):
        for j in range(num_protocols):
            if matrix[i, j] > 0.5:
                color = (0, 0, 0)
            else:
                color = (1, 1, 1)
            _ = ax.text(j, i, f"{matrix[i, j] * 100 :.1f}%", ha="center", va="center", color=color,
                        fontsize="xx-small")
    ax.set_title("Confusion Matrix")
    fig.tight_layout()
    plt.savefig(os.path.join(save_dir, "confusion_matrix.jpg"), dpi=300)
