/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/utils/datastore/pebbledb"
)

func setupTest(t *testing.T) (*pebbledb.DataStore, *Datastore, func()) {
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
	cleanup := func() {
		err := db.Close()
		if err != nil {
			t.Fatal("Failed to close db")
		}
	}

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
	require.NoError(t, err)

	savedTracepoint, err := db.Get("/tracepoint/" + tpID.String())
	require.NoError(t, err)
	savedTracepointPb := &storepb.TracepointInfo{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	require.NoError(t, err)
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

	err = db.Set("/tracepoint/"+tpID.String(), string(s1Text))
	require.NoError(t, err)

	tracepoint, err := ts.GetTracepoint(tpID)
	require.NoError(t, err)
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

	err = db.Set("/tracepoint/"+s1ID.String(), string(s1Text))
	require.NoError(t, err)
	err = db.Set("/tracepoint/"+s2ID.String(), string(s2Text))
	require.NoError(t, err)

	tracepoints, err := ts.GetTracepoints()
	require.NoError(t, err)
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

	err = db.Set("/tracepoint/"+s1ID.String(), string(s1Text))
	require.NoError(t, err)
	err = db.Set("/tracepoint/"+s2ID.String(), string(s2Text))
	require.NoError(t, err)

	tracepoints, err := ts.GetTracepointsForIDs([]uuid.UUID{s1ID, s2ID, s3ID})
	require.NoError(t, err)
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
	require.NoError(t, err)

	savedTracepoint, err := db.Get("/tracepointStates/" + tpID.String() + "/" + agentID.String())
	require.NoError(t, err)
	savedTracepointPb := &storepb.AgentTracepointStatus{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	require.NoError(t, err)
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

	err = db.Set("/tracepointStates/"+tpID.String()+"/"+agentID1.String(), string(s1Text))
	require.NoError(t, err)
	err = db.Set("/tracepointStates/"+tpID.String()+"/"+agentID2.String(), string(s2Text))
	require.NoError(t, err)

	tracepoints, err := ts.GetTracepointStates(tpID)
	require.NoError(t, err)
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
	require.NoError(t, err)

	savedTracepoint, err := db.Get("/tracepointName/test")
	require.NoError(t, err)
	savedTracepointPb := &uuidpb.UUID{}
	err = proto.Unmarshal(savedTracepoint, savedTracepointPb)
	require.NoError(t, err)
	assert.Equal(t, tpID, utils.UUIDFromProtoOrNil(savedTracepointPb))
}

func TestTracepointStore_GetTracepointsWithNames(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())
	tracepointIDpb := utils.ProtoFromUUID(tpID)
	val, err := tracepointIDpb.Marshal()
	require.NoError(t, err)

	tpID2 := uuid.Must(uuid.NewV4())
	tracepointIDpb2 := utils.ProtoFromUUID(tpID2)
	val2, err := tracepointIDpb2.Marshal()
	require.NoError(t, err)

	err = db.Set("/tracepointName/test", string(val))
	require.NoError(t, err)
	err = db.Set("/tracepointName/test2", string(val2))
	require.NoError(t, err)

	tracepoints, err := ts.GetTracepointsWithNames([]string{"test", "test2"})
	require.NoError(t, err)
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

	err := db.Set("/tracepoint/"+tpID.String(), "test")
	require.NoError(t, err)

	err = ts.DeleteTracepoint(tpID)
	require.NoError(t, err)

	val, err := db.Get("/tracepoint/" + tpID.String())
	require.NoError(t, err)
	assert.Nil(t, val)
}

func TestTracepointStore_DeleteTracepointTTLs(t *testing.T) {
	_, ts, cleanup := setupTest(t)
	defer cleanup()

	tpID := uuid.Must(uuid.NewV4())
	tpID2 := uuid.Must(uuid.NewV4())

	err := ts.DeleteTracepointTTLs([]uuid.UUID{tpID, tpID2})
	require.NoError(t, err)
}

func TestTracepointStore_GetTracepointTTLs(t *testing.T) {
	db, ts, cleanup := setupTest(t)
	defer cleanup()

	// Create tracepoints.
	s1ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s2ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c9")

	err := db.Set("/tracepointTTL/"+s1ID.String(), "")
	require.NoError(t, err)
	err = db.Set("/tracepointTTL/"+s2ID.String(), "")
	require.NoError(t, err)
	err = db.Set("/tracepointTTL/invalid", "")
	require.NoError(t, err)

	tracepoints, _, err := ts.GetTracepointTTLs()
	require.NoError(t, err)
	assert.Equal(t, 2, len(tracepoints))

	assert.Contains(t, tracepoints, s1ID)
	assert.Contains(t, tracepoints, s2ID)
}
