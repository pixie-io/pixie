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

package cronscript

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

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
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
	ds := NewDatastore(db)
	cleanup := func() {
		err := db.Close()
		if err != nil {
			t.Fatal("Failed to close db")
		}
	}

	return db, ds, cleanup
}

func TestStore_UpsertCronScript(t *testing.T) {
	db, ds, cleanup := setupTest(t)
	defer cleanup()

	sID := uuid.Must(uuid.NewV4())
	// Create script.
	s1 := &cvmsgspb.CronScript{
		ID:         utils.ProtoFromUUID(sID),
		Script:     "px.display()",
		Configs:    "someConfig",
		FrequencyS: 5,
	}

	err := ds.UpsertCronScript(s1)
	require.NoError(t, err)

	savedScript, err := db.Get("/cronScript/" + sID.String())
	require.NoError(t, err)
	savedPb := &cvmsgspb.CronScript{}
	err = proto.Unmarshal(savedScript, savedPb)
	require.NoError(t, err)
	assert.Equal(t, s1, savedPb)
}

func TestStore_GetCronScripts(t *testing.T) {
	db, ds, cleanup := setupTest(t)
	defer cleanup()

	// Create scripts.
	s1ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s1 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUID(s1ID),
	}
	s1Text, err := s1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal cronscript pb")
	}

	s2ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c9")
	s2 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUID(s2ID),
	}
	s2Text, err := s2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal cronscript pb")
	}

	err = db.Set("/cronScript/"+s1ID.String(), string(s1Text))
	require.NoError(t, err)
	err = db.Set("/cronScript/"+s2ID.String(), string(s2Text))
	require.NoError(t, err)

	scripts, err := ds.GetCronScripts()
	require.NoError(t, err)
	assert.Equal(t, 2, len(scripts))

	ids := make([]string, len(scripts))
	for i, s := range scripts {
		ids[i] = utils.ProtoToUUIDStr(s.ID)
	}

	assert.Contains(t, ids, utils.ProtoToUUIDStr(s1.ID))
	assert.Contains(t, ids, utils.ProtoToUUIDStr(s2.ID))
}

func TestStore_SetCronScripts(t *testing.T) {
	db, ds, cleanup := setupTest(t)
	defer cleanup()

	// Create scripts.
	s1ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s1 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUID(s1ID),
	}
	s1Text, err := s1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal cronscript pb")
	}

	s2ID := uuid.FromStringOrNil("8ba7b810-9dad-11d1-80b4-00c04fd430c9")
	s2 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUID(s2ID),
	}
	s2Text, err := s2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal cronscript pb")
	}

	err = db.Set("/cronScript/"+s1ID.String(), string(s1Text))
	require.NoError(t, err)
	err = db.Set("/cronScript/"+s2ID.String(), string(s2Text))
	require.NoError(t, err)

	// Create new scripts.
	s3ID := uuid.Must(uuid.NewV4())
	s3 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUID(s3ID),
	}
	s4ID := uuid.Must(uuid.NewV4())
	s4 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUID(s4ID),
	}

	err = ds.SetCronScripts([]*cvmsgspb.CronScript{s3, s4})
	require.NoError(t, err)

	scripts, err := ds.GetCronScripts()
	require.NoError(t, err)
	assert.Equal(t, 2, len(scripts))

	ids := make([]string, len(scripts))
	for i, s := range scripts {
		ids[i] = utils.ProtoToUUIDStr(s.ID)
	}

	assert.Contains(t, ids, utils.ProtoToUUIDStr(s3.ID))
	assert.Contains(t, ids, utils.ProtoToUUIDStr(s4.ID))
}
