package gotypes

import (
	"testing"

	"github.com/stretchr/testify/assert"

	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
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
