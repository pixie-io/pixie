package types

import (
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
)

// UInt128 represents a 128 bit unsigned integer which wraps two uint64s.
type UInt128 struct {
	High uint64
	Low  uint64
}

// Equal returns whether or not the UInt128 are equal.
// Copied from https://github.com/cockroachdb/cockroach/blob/c097a16427f65e9070991f062716d222ea5903fe/pkg/util/uint128/uint128.go.
func (u UInt128) Equal(o *UInt128) bool {
	return u.High == o.High && u.Low == o.Low
}

// Compare compares the two UInt128s.
// Copied from https://github.com/cockroachdb/cockroach/blob/c097a16427f65e9070991f062716d222ea5903fe/pkg/util/uint128/uint128.go.
func (u UInt128) Compare(o *UInt128) int {
	if u.High > o.High {
		return 1
	} else if u.High < o.High {
		return -1
	} else if u.Low > o.Low {
		return 1
	} else if u.Low < o.Low {
		return -1
	}
	return 0
}

// UInt128FromProto converts the UInt128 proto into a UInt128.
func UInt128FromProto(pb *typespb.UInt128) *UInt128 {
	return &UInt128{
		High: pb.High,
		Low:  pb.Low,
	}
}

// ProtoFromUInt128 converts the UInt128 into a UInt128 proto.
func ProtoFromUInt128(u *UInt128) *typespb.UInt128 {
	return &typespb.UInt128{
		High: u.High,
		Low:  u.Low,
	}
}
