package controllers_test

import (
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
	mock_kvstore "pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore/mock"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

func TestKVMetadataStore_UpsertTracepoint(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	tpID := uuid.NewV4()
	// Create tracepoints.
	s1 := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(tpID),
	}

	err = mds.UpsertTracepoint(tpID, s1)
	assert.Nil(t, err)

	savedTracepoint, err := c.Get("/tracepoint/" + tpID.String())
	savedTracepointPb := &storepb.TracepointInfo{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	assert.Nil(t, err)
	assert.Equal(t, s1, savedTracepointPb)
}

func TestKVMetadataStore_GetTracepoint(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	tpID := uuid.NewV4()
	// Create tracepoints.
	s1 := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(tpID),
	}
	s1Text, err := s1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal tracepoint pb")
	}

	c.Set("/tracepoint/"+tpID.String(), string(s1Text))

	tracepoint, err := mds.GetTracepoint(tpID)
	assert.Nil(t, err)
	assert.NotNil(t, tracepoint)

	assert.Equal(t, s1.ID, tracepoint.ID)
}

func TestKVMetadataStore_GetTracepoints(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/tracepoint/").
		Return(nil, nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Create tracepoints.
	s1ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s1 := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(s1ID),
	}
	s1Text, err := s1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal tracepoint pb")
	}

	s2ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c9")
	s2 := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(s2ID),
	}
	s2Text, err := s2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal tracepoint pb")
	}

	c.Set("/tracepoint/"+s1ID.String(), string(s1Text))
	c.Set("/tracepoint/"+s2ID.String(), string(s2Text))

	tracepoints, err := mds.GetTracepoints()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(tracepoints))

	assert.Equal(t, s1.ID, tracepoints[0].ID)
	assert.Equal(t, s2.ID, tracepoints[1].ID)
}

func TestKVMetadataStore_GetTracepointsForIDs(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetAll([]string{
			"/tracepoint/8ba7b810-9dad-11d1-80b4-00c04fd430c8",
			"/tracepoint/8ba7b810-9dad-11d1-80b4-00c04fd430c9",
			"/tracepoint/8ba7b810-9dad-11d1-80b4-00c04fd430c7",
		}).
		Return(nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Create tracepoints.
	s1ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s1 := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(s1ID),
	}
	s1Text, err := s1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal tracepoint pb")
	}

	s2ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c9")
	s2 := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(s2ID),
	}
	s2Text, err := s2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal tracepoint pb")
	}

	s3ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c7")

	c.Set("/tracepoint/"+s1ID.String(), string(s1Text))
	c.Set("/tracepoint/"+s2ID.String(), string(s2Text))

	tracepoints, err := mds.GetTracepointsForIDs([]uuid.UUID{s1ID, s2ID, s3ID})
	assert.Nil(t, err)
	assert.Equal(t, 3, len(tracepoints))

	assert.Equal(t, s1.ID, tracepoints[0].ID)
	assert.Equal(t, s2.ID, tracepoints[1].ID)
	assert.Nil(t, tracepoints[2])
}

func TestKVMetadataStore_UpdateTracepointState(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	agentID := uuid.NewV4()
	tpID := uuid.NewV4()
	// Create tracepoint state
	s1 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tpID),
		AgentID: utils.ProtoFromUUID(agentID),
		State:   statuspb.RUNNING_STATE,
	}

	err = mds.UpdateTracepointState(s1)
	assert.Nil(t, err)

	savedTracepoint, err := c.Get("/tracepointStates/" + tpID.String() + "/" + agentID.String())
	savedTracepointPb := &storepb.AgentTracepointStatus{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	assert.Nil(t, err)
	assert.Equal(t, s1, savedTracepointPb)
}

func TestKVMetadataStore_GetTracepointStates(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	tpID := uuid.NewV4()
	mockDs.
		EXPECT().
		GetWithPrefix("/tracepointStates/"+tpID.String()+"/").
		Return(nil, nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	agentID1 := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	agentID2 := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")

	// Create tracepoints.
	s1 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tpID),
		AgentID: utils.ProtoFromUUID(agentID1),
		State:   statuspb.RUNNING_STATE,
	}
	s1Text, err := s1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal tracepoint pb")
	}

	s2 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tpID),
		AgentID: utils.ProtoFromUUID(agentID2),
		State:   statuspb.PENDING_STATE,
	}
	s2Text, err := s2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal tracepoint pb")
	}

	c.Set("/tracepointStates/"+tpID.String()+"/"+agentID1.String(), string(s1Text))
	c.Set("/tracepointStates/"+tpID.String()+"/"+agentID2.String(), string(s2Text))

	tracepoints, err := mds.GetTracepointStates(tpID)
	assert.Nil(t, err)
	assert.Equal(t, 2, len(tracepoints))

	assert.Equal(t, s1.AgentID, tracepoints[0].AgentID)
	assert.Equal(t, s2.AgentID, tracepoints[1].AgentID)
}

func TestKVMetadataStore_SetTracepointWithName(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	tpID := uuid.NewV4()

	err = mds.SetTracepointWithName("test", tpID)
	assert.Nil(t, err)

	savedTracepoint, err := c.Get("/tracepointName/test")
	savedTracepointPb := &uuidpb.UUID{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	assert.Nil(t, err)
	assert.Equal(t, tpID, utils.UUIDFromProtoOrNil(savedTracepointPb))
}

func TestKVMetadataStore_GetTracepointsWithNames(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	tpID := uuid.NewV4()
	tracepointIDpb := utils.ProtoFromUUID(tpID)
	val, err := tracepointIDpb.Marshal()
	assert.Nil(t, err)

	tpID2 := uuid.NewV4()
	tracepointIDpb2 := utils.ProtoFromUUID(tpID2)
	val2, err := tracepointIDpb2.Marshal()
	assert.Nil(t, err)

	c.Set("/tracepointName/test", string(val))
	c.Set("/tracepointName/test2", string(val2))

	tracepoint, err := mds.GetTracepointsWithNames([]string{"test", "test2"})
	assert.Nil(t, err)
	assert.NotNil(t, tracepoint)

	assert.Equal(t, tpID, *tracepoint[0])
	assert.Equal(t, tpID2, *tracepoint[1])
}

func TestKVMetadataStore_DeleteTracepoint(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	tpID := uuid.NewV4()

	mockDs.
		EXPECT().
		DeleteWithPrefix("/tracepointStates/" + tpID.String() + "/").
		Times(1)

	c.Set("/tracepoint/"+tpID.String(), "test")

	err = mds.DeleteTracepoint(tpID)
	assert.Nil(t, err)

	val, err := c.Get("/tracepoint/" + tpID.String())
	assert.Nil(t, err)
	assert.Equal(t, []byte{}, val)
}

func TestKVMetadataStore_DeleteTracepointTTLs(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	tpID := uuid.NewV4()
	tpID2 := uuid.NewV4()

	mockDs.
		EXPECT().
		SetAll([]kvstore.TTLKeyValue{
			kvstore.TTLKeyValue{
				Expire: true,
				TTL:    0,
				Value:  []byte{},
				Key:    "/tracepointTTL/" + tpID.String(),
			},
			kvstore.TTLKeyValue{
				Expire: true,
				TTL:    0,
				Value:  []byte{},
				Key:    "/tracepointTTL/" + tpID2.String(),
			},
		}).
		Return(nil).
		Times(1)

	err = mds.DeleteTracepointTTLs([]uuid.UUID{tpID, tpID2})
	assert.Nil(t, err)
}

func TestKVMetadataStore_WatchTracepointTTLs(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	fakeEvCh := make(chan kvstore.KeyEvent, 2)
	quitCh := make(chan bool, 1)
	defer func() { quitCh <- true }()
	mockDs.
		EXPECT().
		WatchKeyEvents("/tracepointTTL/").
		Return(fakeEvCh, quitCh)

	tpID1 := uuid.NewV4()
	tpID2 := uuid.NewV4()

	go func() {
		fakeEvCh <- kvstore.KeyEvent{EventType: kvstore.EventTypePut, Key: "/tracepointTTL/" + tpID1.String()}
		fakeEvCh <- kvstore.KeyEvent{EventType: kvstore.EventTypeDelete, Key: "/tracepointTTL/" + tpID2.String()}
	}()

	idCh, _ := mds.WatchTracepointTTLs()

	for {
		select {
		case id := <-idCh:
			assert.Equal(t, tpID2, id)
			return
		case <-time.After(2 * time.Second):
			t.Fatal("Timed out waiting for TTL deletion event")
		}
	}
}

func TestKVMetadataStore_GetTracepointTTLs(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/tracepointTTL/").
		Return(nil, nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Create tracepoints.
	s1ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s2ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c9")

	c.Set("/tracepointTTL/"+s1ID.String(), "")
	c.Set("/tracepointTTL/"+s2ID.String(), "")
	c.Set("/tracepointTTL/invalid", "")

	tracepoints, err := mds.GetTracepointTTLs()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(tracepoints))

	assert.Equal(t, s1ID, tracepoints[0])
	assert.Equal(t, s2ID, tracepoints[1])
}
