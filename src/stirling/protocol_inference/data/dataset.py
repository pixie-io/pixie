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

import pandas as pd
from torch.utils.data import Dataset

from model.metadata import kTargetProtocols


class ProtocolDataset(Dataset):
    def __init__(self, dataset_path, transform=None, augmentation=None):
        self.transform = transform
        self.augmentation = augmentation

        # Build a dataframe from each pod folder in memory.
        # TODO(chengruizhe): Might want to load in payload column lazily to conserve memory.
        self.df = pd.read_csv(dataset_path, delimiter='\t', usecols=[0, 1],
                              names=["payload", "protocol"])
        self.protocol2idx = {protocol: i for i, protocol in enumerate(kTargetProtocols)}

    def __len__(self):
        return len(self.df)

    def __getitem__(self, item):
        raise NotImplementedError


class PacketDataset(ProtocolDataset):
    """
    Each row of the dataframe represents a packet on the protocol level.
    """
    def __getitem__(self, idx):
        payload, protocol = self.df.iloc[idx]
        payload = bytes.fromhex(payload)

        if self.transform:
            payload = self.transform(payload)

        if self.augmentation:
            payload = self.augmentation(payload)

        if protocol not in self.protocol2idx:
            return payload, self.protocol2idx["unknown"]

        return payload, self.protocol2idx[protocol]


class ConnDataset(ProtocolDataset):
    """
    Each row of the dataframe represents a series of packets in a connection. Payloads
    are packet payloads separated by ','.
    """
    def __getitem__(self, idx):
        payloads, protocol = self.df.iloc[idx]
        payloads = [bytes.fromhex(payload) for payload in payloads.split(",")]

        if self.transform:
            payloads = [self.transform(payload) for payload in payloads]

        if self.augmentation:
            payloads = [self.augmentation(payload) for payload in payloads]

        if protocol not in self.protocol2idx:
            return payloads, self.protocol2idx["unknown"]

        return payloads, self.protocol2idx[protocol]
