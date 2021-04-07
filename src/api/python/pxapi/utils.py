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

import uuid

from src.api.public.uuidpb import uuid_pb2 as uuidpb


def uuid_pb_from_string(id_str: str) -> uuidpb.UUID:
    u = uuid.UUID(id_str)
    return uuidpb.UUID(
        high_bits=(u.int >> 64),
        low_bits=(u.int & 2**64 - 1),
    )


def uuid_pb_to_string(pb: uuidpb.UUID) -> str:
    if pb.deprecated_data:
        return pb.deprecated_data.decode('utf-8')
    i = (pb.high_bits << 64) + pb.low_bits
    u = uuid.UUID(int=i)
    return str(u)
