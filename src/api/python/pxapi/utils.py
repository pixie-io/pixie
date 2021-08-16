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

import uuid
from authlib.jose import JsonWebKey, RSAKey, JsonWebEncryption

from src.api.proto.uuidpb import uuid_pb2 as uuidpb
from src.api.proto.vizierpb import vizierapi_pb2 as vpb


def uuid_pb_from_string(id_str: str) -> uuidpb.UUID:
    u = uuid.UUID(id_str)
    return uuidpb.UUID(
        high_bits=(u.int >> 64),
        low_bits=(u.int & 2**64 - 1),
    )


def uuid_pb_to_string(pb: uuidpb.UUID) -> str:
    i = (pb.high_bits << 64) + pb.low_bits
    u = uuid.UUID(int=i)
    return str(u)


class CryptoOptions:
    def __init__(self):
        rsa = RSAKey.generate_key(key_size=4096, is_private=True)
        self.jwk_public_key = JsonWebKey.import_key(rsa.as_pem(is_private=False))
        self.jwk_private_key = JsonWebKey.import_key(rsa.as_pem(is_private=True))

        self._key_alg = 'RSA-OAEP-256'
        self._content_alg = 'A256GCM'
        self._compression_alg = 'DEF'

    def encrypt_options(self) -> vpb.ExecuteScriptRequest.EncryptionOptions:
        return vpb.ExecuteScriptRequest.EncryptionOptions(
            jwk_key=self.jwk_public_key.as_json(),
            key_alg=self._key_alg,
            content_alg=self._content_alg,
            compression_alg=self._compression_alg,
        )


def decode_row_batch(crypt: CryptoOptions, data) -> vpb.RowBatchData:
    jwe = JsonWebEncryption()

    rb = vpb.RowBatchData()
    data = jwe.deserialize_compact(data, crypt.jwk_private_key)
    rb.ParseFromString(data['payload'])
    return rb
