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
	"path"
	"strings"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"golang.org/x/sync/errgroup"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/utils/datastore"
)

const (
	tracepointsPrefix      = "/tracepoint/"
	tracepointStatesPrefix = "/tracepointStates/"
	tracepointTTLsPrefix   = "/tracepointTTL/"
	tracepointNamesPrefix  = "/tracepointName/"
)

// Datastore implements the TracepointStore interface on a given Datastore.
type Datastore struct {
	ds datastore.MultiGetterSetterDeleterCloser
}

// NewDatastore wraps the datastore in a tracepointstore
func NewDatastore(ds datastore.MultiGetterSetterDeleterCloser) *Datastore {
	return &Datastore{ds: ds}
}

func getTracepointWithNameKey(tracepointName string) string {
	return path.Join(tracepointNamesPrefix, tracepointName)
}

func getTracepointKey(tracepointID uuid.UUID) string {
	return path.Join(tracepointsPrefix, tracepointID.String())
}

func getTracepointStatesKey(tracepointID uuid.UUID) string {
	return path.Join(tracepointStatesPrefix, tracepointID.String())
}

func getTracepointStateKey(tracepointID uuid.UUID, agentID uuid.UUID) string {
	return path.Join(tracepointStatesPrefix, tracepointID.String(), agentID.String())
}

func getTracepointTTLKey(tracepointID uuid.UUID) string {
	return path.Join(tracepointTTLsPrefix, tracepointID.String())
}

// GetTracepointsWithNames gets which tracepoint is associated with the given name.
func (t *Datastore) GetTracepointsWithNames(tracepointNames []string) ([]*uuid.UUID, error) {
	eg := errgroup.Group{}
	ids := make([]*uuid.UUID, len(tracepointNames))
	for i := 0; i < len(tracepointNames); i++ {
		i := i // Closure for goroutine
		eg.Go(func() error {
			val, err := t.ds.Get(getTracepointWithNameKey(tracepointNames[i]))
			if err != nil {
				return err
			}
			if val == nil {
				return nil
			}
			uuidPB := &uuidpb.UUID{}
			err = proto.Unmarshal(val, uuidPB)
			if err != nil {
				return err
			}
			id := utils.UUIDFromProtoOrNil(uuidPB)
			ids[i] = &id
			return nil
		})
	}
	err := eg.Wait()
	if err != nil {
		return nil, err
	}

	return ids, nil
}

// SetTracepointWithName associates the tracepoint with the given name with the one with the provided ID.
func (t *Datastore) SetTracepointWithName(tracepointName string, tracepointID uuid.UUID) error {
	tracepointIDpb := utils.ProtoFromUUID(tracepointID)
	val, err := tracepointIDpb.Marshal()
	if err != nil {
		return err
	}

	return t.ds.Set(getTracepointWithNameKey(tracepointName), string(val))
}

// UpsertTracepoint updates or creates a new tracepoint entry in the store.
func (t *Datastore) UpsertTracepoint(tracepointID uuid.UUID, tracepointInfo *storepb.TracepointInfo) error {
	val, err := tracepointInfo.Marshal()
	if err != nil {
		return err
	}

	return t.ds.Set(getTracepointKey(tracepointID), string(val))
}

// DeleteTracepoint deletes the tracepoint from the store.
func (t *Datastore) DeleteTracepoint(tracepointID uuid.UUID) error {
	err := t.ds.DeleteAll([]string{getTracepointKey(tracepointID)})
	if err != nil {
		return err
	}

	return t.ds.DeleteWithPrefix(getTracepointStatesKey(tracepointID))
}

// GetTracepoint gets the tracepoint info from the store, if it exists.
func (t *Datastore) GetTracepoint(tracepointID uuid.UUID) (*storepb.TracepointInfo, error) {
	resp, err := t.ds.Get(getTracepointKey(tracepointID))
	if err != nil {
		return nil, err
	}
	if resp == nil {
		return nil, nil
	}

	tracepointPb := &storepb.TracepointInfo{}
	err = proto.Unmarshal(resp, tracepointPb)
	if err != nil {
		return nil, err
	}
	return tracepointPb, nil
}

// GetTracepoints gets all of the tracepoints in the store.
func (t *Datastore) GetTracepoints() ([]*storepb.TracepointInfo, error) {
	_, vals, err := t.ds.GetWithPrefix(tracepointsPrefix)
	if err != nil {
		return nil, err
	}

	tracepoints := make([]*storepb.TracepointInfo, len(vals))
	for i, val := range vals {
		pb := &storepb.TracepointInfo{}
		err := proto.Unmarshal(val, pb)
		if err != nil {
			continue
		}
		tracepoints[i] = pb
	}
	return tracepoints, nil
}

// GetTracepointsForIDs gets all of the tracepoints with the given it.ds.
func (t *Datastore) GetTracepointsForIDs(ids []uuid.UUID) ([]*storepb.TracepointInfo, error) {
	eg := errgroup.Group{}
	tracepoints := make([]*storepb.TracepointInfo, len(ids))
	for i := 0; i < len(ids); i++ {
		i := i // Closure for goroutine
		eg.Go(func() error {
			val, err := t.ds.Get(getTracepointKey(ids[i]))
			if err != nil {
				return err
			}
			if val == nil {
				return nil
			}
			tp := &storepb.TracepointInfo{}
			err = proto.Unmarshal(val, tp)
			if err != nil {
				return err
			}
			tracepoints[i] = tp
			return nil
		})
	}

	err := eg.Wait()
	if err != nil {
		return nil, err
	}

	return tracepoints, nil
}

// UpdateTracepointState updates the agent tracepoint state in the store.
func (t *Datastore) UpdateTracepointState(state *storepb.AgentTracepointStatus) error {
	val, err := state.Marshal()
	if err != nil {
		return err
	}

	tpID := utils.UUIDFromProtoOrNil(state.ID)

	return t.ds.Set(getTracepointStateKey(tpID, utils.UUIDFromProtoOrNil(state.AgentID)), string(val))
}

// GetTracepointStates gets all the agentTracepoint states for the given tracepoint.
func (t *Datastore) GetTracepointStates(tracepointID uuid.UUID) ([]*storepb.AgentTracepointStatus, error) {
	_, vals, err := t.ds.GetWithPrefix(getTracepointStatesKey(tracepointID))
	if err != nil {
		return nil, err
	}

	tracepoints := make([]*storepb.AgentTracepointStatus, len(vals))
	for i, val := range vals {
		pb := &storepb.AgentTracepointStatus{}
		err := proto.Unmarshal(val, pb)
		if err != nil {
			continue
		}
		tracepoints[i] = pb
	}
	return tracepoints, nil
}

// SetTracepointTTL creates a key in the datastore with the given TTL. This represents the amount of time
// that the given tracepoint should be persisted before terminating.
func (t *Datastore) SetTracepointTTL(tracepointID uuid.UUID, ttl time.Duration) error {
	expiresAt := time.Now().Add(ttl)
	encodedExpiry, err := expiresAt.MarshalBinary()
	if err != nil {
		return err
	}
	return t.ds.SetWithTTL(getTracepointTTLKey(tracepointID), string(encodedExpiry), ttl)
}

// DeleteTracepointTTLs deletes the key in the datastore for the given tracepoint TTLs.
// This is done as a single transaction, so if any deletes fail, they all fail.
func (t *Datastore) DeleteTracepointTTLs(ids []uuid.UUID) error {
	keys := make([]string, len(ids))
	for i, id := range ids {
		keys[i] = getTracepointTTLKey(id)
	}

	return t.ds.DeleteAll(keys)
}

// DeleteTracepointsForAgent deletes the tracepoints for a given agent.
// Note this only purges the combo tracepointID+agentID keys. Said
// tracepoints might still be valid and deployed on other agents.
func (t *Datastore) DeleteTracepointsForAgent(agentID uuid.UUID) error {
	tps, err := t.GetTracepoints()
	if err != nil {
		return err
	}

	delKeys := make([]string, len(tps))
	for i, tp := range tps {
		delKeys[i] = getTracepointStateKey(utils.UUIDFromProtoOrNil(tp.ID), agentID)
	}

	return t.ds.DeleteAll(delKeys)
}

// GetTracepointTTLs gets the tracepoints which still have existing TTLs.
func (t *Datastore) GetTracepointTTLs() ([]uuid.UUID, []time.Time, error) {
	keys, vals, err := t.ds.GetWithPrefix(tracepointTTLsPrefix)
	if err != nil {
		return nil, nil, err
	}

	var ids []uuid.UUID
	var expirations []time.Time

	for i, k := range keys {
		keyParts := strings.Split(k, "/")
		if len(keyParts) != 3 {
			continue
		}
		id, err := uuid.FromString(keyParts[2])
		if err != nil {
			continue
		}
		var expiresAt time.Time
		err = expiresAt.UnmarshalBinary(vals[i])
		if err != nil {
			// This shouldn't happen for new keys, but we might have added TTLs
			// in the past without a value. So just pick some time sufficiently
			// in the future.
			// This value is only used to determine what tracepoints are expired
			// as of _NOW_ so this is "safe".
			expiresAt = time.Now().Add(30 * 24 * time.Hour)
		}
		ids = append(ids, id)
		expirations = append(expirations, expiresAt)
	}

	return ids, expirations, nil
}
