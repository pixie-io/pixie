package utils_test

import (
	"bytes"
	"testing"

	"github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	pb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

func TestProtoFromUUID_BaseCaseValidUUID(t *testing.T) {
	u, _ := uuid.FromString("11285cdd-1de9-4ab1-ae6a-0ba08c8c676c")
	proto, err := utils.ProtoFromUUID(&u)
	assert.Nil(t, err, "must not have an error")
	expected := []byte{17, 40, 92, 221, 29, 233, 74, 177, 174, 106, 11, 160, 140, 140, 103, 108}
	assert.Equal(t, expected,
		proto.Data, "must have correct value")
}

func TestProtoFromUUID_NilUUID(t *testing.T) {
	u := uuid.Nil
	proto, err := utils.ProtoFromUUID(&u)
	assert.Nil(t, err, "must not have an error")
	assert.Equal(t, bytes.Repeat([]byte{0}, 16),
		proto.Data, "must have correct value")
}

func TestUUIDFromProto_BaseCaseValidUUID(t *testing.T) {
	proto := &pb.UUID{
		Data: []byte{17, 40, 92, 221, 29, 233, 74, 177, 174, 106, 11, 160, 140, 140, 103, 108},
	}

	u, err := utils.UUIDFromProto(proto)
	assert.Nil(t, err, "must not have an error")
	assert.Equal(t, "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c",
		u.String(), "must have correct value")
}

func TestUUIDFromProto_EmptyUUID(t *testing.T) {
	proto := &pb.UUID{}
	_, err := utils.UUIDFromProto(proto)
	assert.NotNil(t, err)
	assert.Contains(t, err.Error(), "must be exactly 16 bytes")
}

func TestUUIDFromProto_ZeroUUID(t *testing.T) {
	proto := &pb.UUID{
		Data: bytes.Repeat([]byte{0}, 16),
	}
	u, err := utils.UUIDFromProto(proto)
	assert.Nil(t, err, "must not have an error")
	assert.Equal(t, "00000000-0000-0000-0000-000000000000",
		u.String(), "must have correct value")

}
