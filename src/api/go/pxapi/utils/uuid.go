/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package utils

import (
	"encoding/binary"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/api/proto/uuidpb"
)

var enc = binary.BigEndian

// ProtoFromUUIDStrOrNil generates proto from string representation of a UUID (nil value is used if parsing fails).
func ProtoFromUUIDStrOrNil(u string) *uuidpb.UUID {
	data := uuid.FromStringOrNil(u).Bytes()
	p := &uuidpb.UUID{
		HighBits: enc.Uint64(data[0:8]),
		LowBits:  enc.Uint64(data[8:16]),
	}
	return p
}

// ProtoToUUIDStr generates an expensive string representation of a UUID proto.
func ProtoToUUIDStr(pb *uuidpb.UUID) string {
	b := make([]byte, 16)
	enc.PutUint64(b[0:], pb.HighBits)
	enc.PutUint64(b[8:], pb.LowBits)
	u, err := uuid.FromBytes(b)
	if err != nil {
		return uuid.Nil.String()
	}
	return u.String()
}
