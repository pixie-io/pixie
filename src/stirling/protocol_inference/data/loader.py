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
from torch.utils.data import DataLoader

from data.dataset import PacketDataset, ConnDataset


def simple_collate(batch):
    """
    This collate doesn't check to ensure that the each element in the batch has equal size.
    This is useful in loading packet and connection data where payloads are just strings,
    not tensors.
    """
    payloads = []
    targets = []
    for payload, target in batch:
        payloads.append(payload)
        targets.append(target)
    targets = torch.LongTensor(targets)
    return payloads, targets


def build_loader(args):
    if "conn" in args.dataset:
        dataset = ConnDataset(args.dataset)
    else:
        dataset = PacketDataset(args.dataset)
    loader = DataLoader(dataset, batch_size=args.batch, num_workers=args.num_workers, shuffle=False,
                        collate_fn=simple_collate)
    return loader
