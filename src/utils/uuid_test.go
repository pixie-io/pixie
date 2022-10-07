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

package utils_test

import (
	"encoding/binary"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/utils"
)

const uuidStr = "ea8aa095-697f-49f1-b127-d50e5b6e2645"
const hi uint64 = 0xea8aa095697f49f1
const lo uint64 = 0xb127d50e5b6e2645

var enc = binary.BigEndian

func TestProtoFromUUID_BaseCaseValidUUID(t *testing.T) {
	u := uuid.FromStringOrNil(uuidStr)
	proto := utils.ProtoFromUUID(u)

	assert.Equal(t, hi, proto.HighBits)
	assert.Equal(t, lo, proto.LowBits)
}

func TestProtoFromUUIDStrOrNil_ValidUUID(t *testing.T) {
	proto := utils.ProtoFromUUIDStrOrNil(uuidStr)
	assert.Equal(t, hi, proto.HighBits)
	assert.Equal(t, lo, proto.LowBits)
}

func TestProtoFromUUIDStrOrNil_InValidUUID(t *testing.T) {
	// The 1 is removed from 4th segment b127.
	uuidStr := "ea8aa095-697f-49f1-b27-d50e5b6e2645"
	proto := utils.ProtoFromUUIDStrOrNil(uuidStr)

	assert.Equal(t, uint64(0), proto.HighBits)
	assert.Equal(t, uint64(0), proto.LowBits)
}

func TestUUIDFromProto_BitsValidUUID(t *testing.T) {
	proto := &uuidpb.UUID{
		HighBits: hi,
		LowBits:  lo,
	}

	u, err := utils.UUIDFromProto(proto)
	require.NoError(t, err, "must not have an error")
	assert.Equal(t, uuidStr, u.String(), "must have correct value")

	wire, err := proto.Marshal()
	require.NoError(t, err)
	t.Logf("Wire Size for New Format: %d bytes", len(wire))
}

func TestUUIDFromProto_EmptyUUID(t *testing.T) {
	proto := &uuidpb.UUID{}
	_, err := utils.UUIDFromProto(proto)
	assert.NotNil(t, err)
	assert.Contains(t, err.Error(), "uuid data in proto is nil")
}

func TestUUIDSame_NilNil(t *testing.T) {
	p1 := &uuidpb.UUID{}
	p2 := &uuidpb.UUID{}
	assert.True(t, utils.AreSameUUID(p1, p2))
}

func TestUUIDSame_NilNotNil(t *testing.T) {
	p1 := &uuidpb.UUID{}
	p2 := &uuidpb.UUID{
		HighBits: hi,
		LowBits:  lo,
	}
	assert.False(t, utils.AreSameUUID(p1, p2))
}

func TestUUIDSame_Same(t *testing.T) {
	p1 := &uuidpb.UUID{
		HighBits: hi,
		LowBits:  lo,
	}
	p2 := &uuidpb.UUID{
		HighBits: hi,
		LowBits:  lo,
	}
	assert.True(t, utils.AreSameUUID(p1, p2))
}

func TestUUIDSame_Different(t *testing.T) {
	p1 := &uuidpb.UUID{
		HighBits: hi,
		LowBits:  lo,
	}
	p2 := &uuidpb.UUID{
		HighBits: lo,
		LowBits:  hi,
	}
	assert.False(t, utils.AreSameUUID(p1, p2))
}

func BenchmarkUUIDFromString(b *testing.B) {
	for i := 0; i < b.N; i++ {
		uuid.FromStringOrNil(uuidStr)
	}
}

func BenchmarkUUIDFromBytes(b *testing.B) {
	for i := 0; i < b.N; i++ {
		data := make([]byte, 16)
		enc.PutUint64(data[0:], hi)
		enc.PutUint64(data[8:], lo)
		uuid.FromBytesOrNil(data)
	}
}

func BenchmarkUUIDToString(b *testing.B) {
	u := uuid.Must(uuid.NewV4())
	for i := 0; i < b.N; i++ {
		_ = u.String()
	}
}

func BenchmarkUUIDToBytes(b *testing.B) {
	u := uuid.Must(uuid.NewV4())
	for i := 0; i < b.N; i++ {
		data := u.Bytes()
		enc.Uint64(data[0:8])
		enc.Uint64(data[8:16])
	}
}
