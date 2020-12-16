package utils_test

import (
	"testing"

	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	pb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

func TestProtoFromUUID_BaseCaseValidUUID(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, _ := uuid.FromString(uuidStr)
	proto := utils.ProtoFromUUID(u)
	expected := []byte(uuidStr)
	assert.Equal(t, expected,
		proto.Data, "must have correct value")
}

func TestProtoFromUUID_NilUUID(t *testing.T) {
	u := uuid.Nil

	proto := utils.ProtoFromUUID(u)
	assert.Equal(t, []byte(uuid.Nil.String()),
		proto.Data, "must have correct value")
}

func TestProtoFromUUIDStrOrNil_ValidUUID(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	proto := utils.ProtoFromUUIDStrOrNil(uuidStr)
	assert.Equal(t, []byte(uuid.FromStringOrNil(uuidStr).String()),
		proto.Data, "must have correct value")
}

func TestProtoFromUUIDStrOrNil_InValidUUID(t *testing.T) {
	// Missing first char.
	uuidStr := "1285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	proto := utils.ProtoFromUUIDStrOrNil(uuidStr)
	assert.Equal(t, []byte(uuid.Nil.String()),
		proto.Data, "must have nil value")
}

func TestUUIDFromProto_BaseCaseValidUUID(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	proto := &pb.UUID{
		Data: []byte(uuidStr),
	}

	u, err := utils.UUIDFromProto(proto)
	assert.Nil(t, err, "must not have an error")
	assert.Equal(t, uuidStr,
		u.String(), "must have correct value")
}

func TestUUIDFromProto_EmptyUUID(t *testing.T) {
	proto := &pb.UUID{}
	_, err := utils.UUIDFromProto(proto)
	assert.NotNil(t, err)
	assert.Contains(t, err.Error(), "incorrect UUID length")
}

func TestUUIDFromProto_ZeroUUID(t *testing.T) {
	proto := &pb.UUID{
		Data: []byte(uuid.Nil.String()),
	}
	u, err := utils.UUIDFromProto(proto)
	assert.Nil(t, err, "must not have an error")
	assert.Equal(t, "00000000-0000-0000-0000-000000000000",
		u.String(), "must have correct value")
}

func TestUUIDFromProtoOrNil_ValidUUID(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	proto := &pb.UUID{
		Data: []byte(uuidStr),
	}

	u := utils.UUIDFromProtoOrNil(proto)
	assert.Equal(t, uuidStr,
		u.String(), "must have correct value")
}

func TestUUIDFromProtoOrNil_InValidUUID(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c6ac"
	proto := &pb.UUID{
		Data: []byte(uuidStr),
	}

	u := utils.UUIDFromProtoOrNil(proto)
	assert.Equal(t, uuid.Nil.String(),
		u.String(), "must have nil value")
}
