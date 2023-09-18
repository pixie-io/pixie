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
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"sort"
	"strings"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/api/proto/uuidpb"
)

var enc = binary.BigEndian

// UUIDFromProto converts a proto message to uuid.
func UUIDFromProto(pb *uuidpb.UUID) (uuid.UUID, error) {
	if pb == nil {
		return uuid.Nil, errors.New("nil proto given")
	}
	if pb.HighBits != 0 && pb.LowBits != 0 {
		b := make([]byte, 16)
		enc.PutUint64(b[0:], pb.HighBits)
		enc.PutUint64(b[8:], pb.LowBits)
		return uuid.FromBytes(b)
	}
	return uuid.Nil, errors.New("uuid data in proto is nil")
}

// UUIDFromProtoOrNil converts a proto message to uuid if error sets to nil uuid.
func UUIDFromProtoOrNil(pb *uuidpb.UUID) uuid.UUID {
	u, _ := UUIDFromProto(pb)
	return u
}

// ProtoFromUUID converts a UUID to proto.
func ProtoFromUUID(u uuid.UUID) *uuidpb.UUID {
	data := u.Bytes()
	p := &uuidpb.UUID{
		HighBits: enc.Uint64(data[0:8]),
		LowBits:  enc.Uint64(data[8:16]),
	}
	return p
}

// ProtoFromUUIDStrOrNil generates proto from string representation of a UUID (nil value is used if parsing fails).
func ProtoFromUUIDStrOrNil(u string) *uuidpb.UUID {
	return ProtoFromUUID(uuid.FromStringOrNil(u))
}

// ProtoToUUIDStr generates an expensive string representation of a UUID proto.
func ProtoToUUIDStr(pb *uuidpb.UUID) string {
	return UUIDFromProtoOrNil(pb).String()
}

// IsNilUUID tells you if the given UUID is nil.
func IsNilUUID(u uuid.UUID) bool {
	return u == uuid.Nil
}

// IsNilUUIDProto tells you if the given UUID is nil.
func IsNilUUIDProto(pb *uuidpb.UUID) bool {
	if pb == nil {
		return true
	}
	if pb.HighBits != 0 && pb.LowBits != 0 {
		return false
	}
	return true
}

// AreSameUUID tells you if the two protos refer to the same underlying UUID.
func AreSameUUID(pb1, pb2 *uuidpb.UUID) bool {
	return pb1.HighBits == pb2.HighBits && pb1.LowBits == pb2.LowBits
}

// HashUUIDs generates a unique hash for a list of UUIDs.
func HashUUIDs(ids []uuid.UUID) string {
	// Sort the UUIDs to ensure consistent ordering.
	sort.Slice(ids, func(i, j int) bool {
		return ids[i].String() < ids[j].String()
	})

	// Convert the sorted UUIDs to a single string.
	concatenated := strings.Join(uuidsToStrings(ids), "")

	// Compute the SHA-256 hash of the concatenated string.
	hash := sha256.Sum256([]byte(concatenated))

	return hex.EncodeToString(hash[:])
}

// uuidsToStrings converts a slice of UUIDs to a slice of their string representations.
func uuidsToStrings(ids []uuid.UUID) []string {
	strs := make([]string, len(ids))
	for i, id := range ids {
		strs[i] = id.String()
	}
	return strs
}
