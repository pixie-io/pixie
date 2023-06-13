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

import argparse
from dataclasses import dataclass
from enum import Enum
from typing import Any, TextIO


def check_positive(arg) -> int:
    iarg = int(arg)
    if iarg <= 0:
        raise argparse.ArgumentTypeError(f"{arg} must be a positive int")
    return iarg


def check_percentage(arg) -> int:
    iarg = int(arg)
    if iarg < 0 or iarg > 99:
        raise argparse.ArgumentTypeError(
            f"{arg} must be a valid percentage between 0 and 99")
    return iarg


class PrivyFileType(Enum):
    """Enum for the different types of http parameters that can be generated.
    Used to keep track of the pii types generated for each payload."""
    PAYLOADS = 1
    TEMPLATES = 2
    SPANS = 3


@dataclass()
class PrivyWriter:
    """PrivyWriter holds the open file and csv writer for output data files."""
    file_type: PrivyFileType
    open_file: TextIO
    csv_writer: Any
