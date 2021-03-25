package tracepoint

import (
	"os"
	"testing"
	"time"

	"github.com/cockroachdb/pebble"
	"github.com/cockroachdb/pebble/vfs"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/api/public/uuidpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	"pixielabs.ai/pixielabs/src/vizier/utils/datastore/pebbledb"
)

func setupTest(t *testing.T) (*pebbledb.DataStore, *Datastore, func() error) {
	memFS := vfs.NewMem()
	c, err := pebble.Open("test", &pebble.Options{
		FS: memFS,
	})
	if err != nil {
		t.Fatal("failed to initialize a pebbledb")
		os.Exit(1)
	}

	db := pebbledb.New(c, 3*time.Second)
	ts := NewDatastore(db)
	cleanup := db.Close

	return db, ts, cleanup
}

func TestTracepointStore_UpsertTracepoint(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())
	// Create tracepoints.
	s1 := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(tpID),
	}

	err := ts.UpsertTracepoint(tpID, s1)
	assert.Nil(t, err)

	savedTracepoint, err := db.Get("/tracepoint/" + tpID.String())
	savedTracepointPb := &storepb.TracepointInfo{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	assert.Nil(t, err)
	assert.Equal(t, s1, savedTracepointPb)
}

func TestTracepointStore_GetTracepoint(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())
	// Create tracepoints.
	s1 := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(tpID),
	}
	s1Text, err := s1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal tracepoint pb")
	}

	db.Set("/tracepoint/"+tpID.String(), string(s1Text))

	tracepoint, err := ts.GetTracepoint(tpID)
	assert.Nil(t, err)
	assert.NotNil(t, tracepoint)

	assert.Equal(t, s1.ID, tracepoint.ID)
}

func TestTracepointStore_GetTracepoints(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

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

	db.Set("/tracepoint/"+s1ID.String(), string(s1Text))
	db.Set("/tracepoint/"+s2ID.String(), string(s2Text))

	tracepoints, err := ts.GetTracepoints()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(tracepoints))

	ids := make([]string, len(tracepoints))
	for i, tp := range tracepoints {
		ids[i] = utils.ProtoToUUIDStr(tp.ID)
	}

	assert.Contains(t, ids, utils.ProtoToUUIDStr(s1.ID))
	assert.Contains(t, ids, utils.ProtoToUUIDStr(s2.ID))
}

func TestTracepointStore_GetTracepointsForIDs(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

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

	db.Set("/tracepoint/"+s1ID.String(), string(s1Text))
	db.Set("/tracepoint/"+s2ID.String(), string(s2Text))

	tracepoints, err := ts.GetTracepointsForIDs([]uuid.UUID{s1ID, s2ID, s3ID})
	assert.Nil(t, err)
	assert.Equal(t, 3, len(tracepoints))

	ids := make([]string, len(tracepoints))
	for i, tp := range tracepoints {
		if tp == nil || tp.ID == nil {
			continue
		}
		ids[i] = utils.ProtoToUUIDStr(tp.ID)
	}

	assert.Contains(t, ids, utils.ProtoToUUIDStr(s1.ID))
	assert.Contains(t, ids, utils.ProtoToUUIDStr(s2.ID))
}

func TestTracepointStore_UpdateTracepointState(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	agentID := uuid.Must(uuid.NewV4())
	tpID := uuid.Must(uuid.NewV4())
	// Create tracepoint state
	s1 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tpID),
		AgentID: utils.ProtoFromUUID(agentID),
		State:   statuspb.RUNNING_STATE,
	}

	err := ts.UpdateTracepointState(s1)
	assert.Nil(t, err)

	savedTracepoint, err := db.Get("/tracepointStates/" + tpID.String() + "/" + agentID.String())
	savedTracepointPb := &storepb.AgentTracepointStatus{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	assert.Nil(t, err)
	assert.Equal(t, s1, savedTracepointPb)
}

func TestTracepointStore_GetTracepointStates(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())

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

	db.Set("/tracepointStates/"+tpID.String()+"/"+agentID1.String(), string(s1Text))
	db.Set("/tracepointStates/"+tpID.String()+"/"+agentID2.String(), string(s2Text))

	tracepoints, err := ts.GetTracepointStates(tpID)
	assert.Nil(t, err)
	assert.Equal(t, 2, len(tracepoints))

	agentIDs := make([]string, len(tracepoints))
	for i, tp := range tracepoints {
		agentIDs[i] = utils.ProtoToUUIDStr(tp.AgentID)
	}

	assert.Contains(t, agentIDs, utils.ProtoToUUIDStr(s1.AgentID))
	assert.Contains(t, agentIDs, utils.ProtoToUUIDStr(s2.AgentID))
}

func TestTracepointStore_SetTracepointWithName(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())

	err := ts.SetTracepointWithName("test", tpID)
	assert.Nil(t, err)

	savedTracepoint, err := db.Get("/tracepointName/test")
	savedTracepointPb := &uuidpb.UUID{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	assert.Nil(t, err)
	assert.Equal(t, tpID, utils.UUIDFromProtoOrNil(savedTracepointPb))
}

func TestTracepointStore_GetTracepointsWithNames(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())
	tracepointIDpb := utils.ProtoFromUUID(tpID)
	val, err := tracepointIDpb.Marshal()
	assert.Nil(t, err)

	tpID2 := uuid.Must(uuid.NewV4())
	tracepointIDpb2 := utils.ProtoFromUUID(tpID2)
	val2, err := tracepointIDpb2.Marshal()
	assert.Nil(t, err)

	db.Set("/tracepointName/test", string(val))
	db.Set("/tracepointName/test2", string(val2))

	tracepoints, err := ts.GetTracepointsWithNames([]string{"test", "test2"})
	assert.Nil(t, err)
	assert.Equal(t, 2, len(tracepoints))

	tps := make([]string, len(tracepoints))
	for i, tp := range tracepoints {
		tps[i] = tp.String()
	}

	assert.Contains(t, tps, tpID.String())
	assert.Contains(t, tps, tpID2.String())
}

func TestTracepointStore_DeleteTracepoint(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())

	db.Set("/tracepoint/"+tpID.String(), "test")

	err := ts.DeleteTracepoint(tpID)
	assert.Nil(t, err)

	val, err := db.Get("/tracepoint/" + tpID.String())
	assert.Nil(t, err)
	assert.Nil(t, val)
}

func TestTracepointStore_DeleteTracepointTTLs(t *testing.T) {
	_, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())
	tpID2 := uuid.Must(uuid.NewV4())

	err := ts.DeleteTracepointTTLs([]uuid.UUID{tpID, tpID2})
	assert.Nil(t, err)
}

func TestTracepointStore_GetTracepointTTLs(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	// Create tracepoints.
	s1ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s2ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c9")

	db.Set("/tracepointTTL/"+s1ID.String(), "")
	db.Set("/tracepointTTL/"+s2ID.String(), "")
	db.Set("/tracepointTTL/invalid", "")

	tracepoints, _, err := ts.GetTracepointTTLs()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(tracepoints))

	assert.Contains(t, tracepoints, s1ID)
	assert.Contains(t, tracepoints, s2ID)
}
