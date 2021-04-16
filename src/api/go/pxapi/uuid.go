package pxapi

import (
	"encoding/binary"

	"github.com/gofrs/uuid"

	pb "px.dev/pixie/src/api/public/uuidpb"
)

var enc = binary.BigEndian

// ProtoFromUUIDStrOrNil generates proto from string representation of a UUID (nil value is used if parsing fails).
func ProtoFromUUIDStrOrNil(u string) *pb.UUID {
	data := uuid.FromStringOrNil(u).Bytes()
	p := &pb.UUID{
		HighBits: enc.Uint64(data[0:8]),
		LowBits:  enc.Uint64(data[8:16]),
	}
	return p
}

// ProtoToUUIDStr generates an expensive string representation of a UUID proto.
func ProtoToUUIDStr(pb *pb.UUID) string {
	b := make([]byte, 16)
	enc.PutUint64(b[0:], pb.HighBits)
	enc.PutUint64(b[8:], pb.LowBits)
	u, err := uuid.FromBytes(b)
	if err != nil {
		return uuid.Nil.String()
	}
	return u.String()
}
