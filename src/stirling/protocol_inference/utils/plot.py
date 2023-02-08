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

from copy import copy

from model.metadata import kTargetProtocols
from prettytable import PrettyTable
from termcolor import colored


def plot_confusion_matrix(matrix):
    num_protocols = len(kTargetProtocols)

    total_packets = matrix.sum(axis=1)

    # Normalize each row of the matrix.
    matrix = matrix / (total_packets + 1e-8)[:, None]

    columns = copy(kTargetProtocols)
    columns.insert(0, "Y axis")

    table = PrettyTable()
    table.field_names = columns

    first_column = [f"{proto}({int(num)})" for proto, num in zip(kTargetProtocols, total_packets)]

    for i in range(num_protocols):
        columns = [first_column[i]]
        for j in range(num_protocols):
            elem = f"{matrix[i, j] * 100 :.1f}%"
            if matrix[i, j] > 0.5:
                columns.append(colored(elem, "yellow"))
            else:
                columns.append(elem)

        table.add_row(columns)
    print(table)
