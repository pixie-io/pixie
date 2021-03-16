package utils

import (
	"github.com/gofrs/uuid"
	pb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
)

// UUIDFromProto converts a proto message to uuid.
func UUIDFromProto(pb *pb.UUID) (uuid.UUID, error) {
	return uuid.FromString(string(pb.Data))
}

// UUIDFromProtoOrNil converts a proto message to uuid if error sets to nil uuid.
func UUIDFromProtoOrNil(pb *pb.UUID) uuid.UUID {
	if pb == nil {
		return uuid.Nil
	}
	return uuid.FromStringOrNil(string(pb.Data))
}

// ProtoFromUUID converts a UUID to proto.
func ProtoFromUUID(u uuid.UUID) *pb.UUID {
	p := &pb.UUID{
		Data: []byte(u.String()),
	}
	return p
}

// ProtoFromUUIDStrOrNil generates proto from string representation of a UUID (nil value is used if parsing fails).
func ProtoFromUUIDStrOrNil(u string) *pb.UUID {
	uid := uuid.FromStringOrNil(u)
	return ProtoFromUUID(uid)
}
