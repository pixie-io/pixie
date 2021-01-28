package controllers

import (
	"path"
	"strings"
	"time"

	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"

	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	"pixielabs.ai/pixielabs/src/utils"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	"pixielabs.ai/pixielabs/src/vizier/utils/datastore"
)

const (
	tracepointsPrefix      = "/tracepoint"
	tracepointStatesPrefix = "/tracepointStates"
	tracepointTTLsPrefix   = "/tracepointTTL"
	tracepointNamesPrefix  = "/tracepointName"
)

// TracepointDatastore implements the TracepointStore interface on a given Datastore.
type TracepointDatastore struct {
	ds datastore.MultiGetterSetterDeleterCloser
}

// NewTracepointDatastore wraps the datastore in a tracepointstore
func NewTracepointDatastore(ds datastore.MultiGetterSetterDeleterCloser) *TracepointDatastore {
	return &TracepointDatastore{ds: ds}
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
func (t *TracepointDatastore) GetTracepointsWithNames(tracepointNames []string) ([]*uuid.UUID, error) {
	keys := make([]string, len(tracepointNames))
	for i, n := range tracepointNames {
		keys[i] = getTracepointWithNameKey(n)
	}

	resp, err := t.ds.GetAll(keys)
	if err != nil {
		return nil, err
	}

	ids := make([]*uuid.UUID, len(keys))
	for i, r := range resp {
		if r != nil {
			uuidPB := &uuidpb.UUID{}
			err = proto.Unmarshal(r, uuidPB)
			if err != nil {
				return nil, err
			}
			id := utils.UUIDFromProtoOrNil(uuidPB)
			ids[i] = &id
		}
	}

	return ids, nil
}

// SetTracepointWithName associates the tracepoint with the given name with the one with the provided ID.
func (t *TracepointDatastore) SetTracepointWithName(tracepointName string, tracepointID uuid.UUID) error {
	tracepointIDpb := utils.ProtoFromUUID(tracepointID)
	val, err := tracepointIDpb.Marshal()
	if err != nil {
		return err
	}

	t.ds.Set(getTracepointWithNameKey(tracepointName), string(val))
	return nil
}

// UpsertTracepoint updates or creates a new tracepoint entry in the store.
func (t *TracepointDatastore) UpsertTracepoint(tracepointID uuid.UUID, tracepointInfo *storepb.TracepointInfo) error {
	val, err := tracepointInfo.Marshal()
	if err != nil {
		return err
	}

	t.ds.Set(getTracepointKey(tracepointID), string(val))
	return nil
}

// DeleteTracepoint deletes the tracepoint from the store.
func (t *TracepointDatastore) DeleteTracepoint(tracepointID uuid.UUID) error {
	t.ds.DeleteAll([]string{getTracepointKey(tracepointID)})

	return t.ds.DeleteWithPrefix(getTracepointStatesKey(tracepointID))
}

// GetTracepoint gets the tracepoint info from the store, if it exists.
func (t *TracepointDatastore) GetTracepoint(tracepointID uuid.UUID) (*storepb.TracepointInfo, error) {
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
func (t *TracepointDatastore) GetTracepoints() ([]*storepb.TracepointInfo, error) {
	_, vals, err := t.ds.GetWithPrefix(tracepointsPrefix)
	if err != nil {
		return nil, err
	}

	tracepoints := make([]*storepb.TracepointInfo, len(vals))
	for i, val := range vals {
		pb := &storepb.TracepointInfo{}
		proto.Unmarshal(val, pb)
		tracepoints[i] = pb
	}
	return tracepoints, nil
}

// GetTracepointsForIDs gets all of the tracepoints with the given it.ds.
func (t *TracepointDatastore) GetTracepointsForIDs(ids []uuid.UUID) ([]*storepb.TracepointInfo, error) {
	keys := make([]string, len(ids))
	for i, id := range ids {
		keys[i] = getTracepointKey(id)
	}

	vals, err := t.ds.GetAll(keys)
	if err != nil {
		return nil, err
	}

	tracepoints := make([]*storepb.TracepointInfo, len(vals))
	for i, val := range vals {
		if val == nil {
			tracepoints[i] = nil
			continue
		}
		pb := &storepb.TracepointInfo{}
		proto.Unmarshal(val, pb)
		tracepoints[i] = pb
	}
	return tracepoints, nil
}

// UpdateTracepointState updates the agent tracepoint state in the store.
func (t *TracepointDatastore) UpdateTracepointState(state *storepb.AgentTracepointStatus) error {
	val, err := state.Marshal()
	if err != nil {
		return err
	}

	tpID := utils.UUIDFromProtoOrNil(state.ID)

	t.ds.Set(getTracepointStateKey(tpID, utils.UUIDFromProtoOrNil(state.AgentID)), string(val))
	return nil
}

// GetTracepointStates gets all the agentTracepoint states for the given tracepoint.
func (t *TracepointDatastore) GetTracepointStates(tracepointID uuid.UUID) ([]*storepb.AgentTracepointStatus, error) {
	_, vals, err := t.ds.GetWithPrefix(getTracepointStatesKey(tracepointID))
	if err != nil {
		return nil, err
	}

	tracepoints := make([]*storepb.AgentTracepointStatus, len(vals))
	for i, val := range vals {
		pb := &storepb.AgentTracepointStatus{}
		proto.Unmarshal(val, pb)
		tracepoints[i] = pb
	}
	return tracepoints, nil
}

// SetTracepointTTL creates a key in the datastore with the given TTL. This represents the amount of time
// that the given tracepoint should be persisted before terminating.
func (t *TracepointDatastore) SetTracepointTTL(tracepointID uuid.UUID, ttl time.Duration) error {
	expiresAt := time.Now().Add(ttl)
	encodedExpiry, err := expiresAt.MarshalBinary()
	if err != nil {
		return err
	}
	return t.ds.SetWithTTL(getTracepointTTLKey(tracepointID), string(encodedExpiry), ttl)
}

// DeleteTracepointTTLs deletes the key in the datastore for the given tracepoint TTLs.
// This is done as a single transaction, so if any deletes fail, they all fail.
func (t *TracepointDatastore) DeleteTracepointTTLs(ids []uuid.UUID) error {
	keys := make([]string, len(ids))
	for i, id := range ids {
		keys[i] = getTracepointTTLKey(id)
	}

	return t.ds.DeleteAll(keys)
}

// DeleteTracepointsForAgent deletes the tracepoints for a given agent.
// Note this only purges the combo tracepointID+agentID keys. Said
// tracepoints might still be valid and deployed on other agents.
func (t *TracepointDatastore) DeleteTracepointsForAgent(agentID uuid.UUID) error {
	tps, err := t.GetTracepoints()
	if err != nil {
		return err
	}

	delKeys := make([]string, len(tps))
	for _, tp := range tps {
		delKeys = append(delKeys, getTracepointStateKey(utils.UUIDFromProtoOrNil(tp.ID), agentID))
	}

	return t.ds.DeleteAll(delKeys)
}

// GetTracepointTTLs gets the tracepoints which still have existing TTLs.
func (t *TracepointDatastore) GetTracepointTTLs() ([]uuid.UUID, []time.Time, error) {
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
