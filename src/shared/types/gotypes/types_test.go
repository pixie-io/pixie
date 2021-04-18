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

package gotypes

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/shared/types/typespb"
)

func TestUInt128Equal(t *testing.T) {
	uint1 := UInt128{
		High: uint64(123),
		Low:  uint64(456),
	}

	uint2 := UInt128{
		High: uint64(123),
		Low:  uint64(456),
	}

	uint3 := UInt128{
		High: uint64(124),
		Low:  uint64(456),
	}

	assert.Equal(t, true, uint1.Equal(&uint2))
	assert.Equal(t, false, uint1.Equal(&uint3))
}

func TestUInt128Compare(t *testing.T) {
	uint1 := UInt128{
		High: uint64(123),
		Low:  uint64(456),
	}

	uint2 := UInt128{
		High: uint64(123),
		Low:  uint64(456),
	}

	uint3 := UInt128{
		High: uint64(124),
		Low:  uint64(456),
	}

	uint4 := UInt128{
		High: uint64(124),
		Low:  uint64(457),
	}

	assert.Equal(t, 0, uint1.Compare(&uint2))
	assert.Equal(t, -1, uint1.Compare(&uint3))
	assert.Equal(t, 1, uint3.Compare(&uint1))
	assert.Equal(t, -1, uint1.Compare(&uint4))
	assert.Equal(t, 1, uint4.Compare(&uint1))
}

func TestUInt128FromProto(t *testing.T) {
	uintPb := &typespb.UInt128{
		High: uint64(123),
		Low:  uint64(456),
	}

	uint1 := UInt128FromProto(uintPb)
	assert.Equal(t, uint64(123), uint1.High)
	assert.Equal(t, uint64(456), uint1.Low)
}

func TestProtoFromUInt128(t *testing.T) {
	uint1 := &UInt128{
		High: uint64(123),
		Low:  uint64(456),
	}

	uintPb := ProtoFromUInt128(uint1)
	assert.Equal(t, uint64(123), uintPb.High)
	assert.Equal(t, uint64(456), uintPb.Low)
}
