package utils

import (
	"github.com/satori/go.uuid"
	pb "pixielabs.ai/pixielabs/src/utils/proto"
)

// UUIDFromProto converts a proto message to uuid.
func UUIDFromProto(pb *pb.UUID) (uuid.UUID, error) {
	return uuid.FromBytes(pb.Data)
}

// ProtoFromUUID converts a UUID to proto.
func ProtoFromUUID(u *uuid.UUID) (*pb.UUID, error) {
	p := &pb.UUID{
		Data: u.Bytes(),
	}
	return p, nil
}
