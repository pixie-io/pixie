package utils

import (
	"encoding/binary"
	"errors"

	"github.com/gofrs/uuid"

	pb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
)

var enc = binary.BigEndian

// UUIDFromProto converts a proto message to uuid.
func UUIDFromProto(pb *pb.UUID) (uuid.UUID, error) {
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
func UUIDFromProtoOrNil(pb *pb.UUID) uuid.UUID {
	u, _ := UUIDFromProto(pb)
	return u
}

// ProtoFromUUID converts a UUID to proto.
func ProtoFromUUID(u uuid.UUID) *pb.UUID {
	data := u.Bytes()
	p := &pb.UUID{
		HighBits: enc.Uint64(data[0:8]),
		LowBits:  enc.Uint64(data[8:16]),
	}
	return p
}

// ProtoFromUUIDStrOrNil generates proto from string representation of a UUID (nil value is used if parsing fails).
func ProtoFromUUIDStrOrNil(u string) *pb.UUID {
	return ProtoFromUUID(uuid.FromStringOrNil(u))
}

// ProtoToUUIDStr generates an expensive string representation of a UUID proto.
func ProtoToUUIDStr(pb *pb.UUID) string {
	return UUIDFromProtoOrNil(pb).String()
}

// IsNilUUID tells you if the given UUID is nil.
func IsNilUUID(u uuid.UUID) bool {
	return u == uuid.Nil
}

// IsNilUUIDProto tells you if the given UUID is nil.
func IsNilUUIDProto(pb *pb.UUID) bool {
	if pb == nil {
		return true
	}
	if pb.HighBits != 0 && pb.LowBits != 0 {
		return false
	}
	return true
}
