package controllers

import (
	"path"
	"strings"
	"time"

	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"

	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

func getTracepointWithNameKey(tracepointName string) string {
	return path.Join("/", "tracepointName", tracepointName)
}

func getTracepointsKey() string {
	return path.Join("/", "tracepoint") + "/"
}

func getTracepointKey(tracepointID uuid.UUID) string {
	return path.Join("/", "tracepoint", tracepointID.String())
}

func getTracepointStatesKey(tracepointID uuid.UUID) string {
	return path.Join("/", "tracepointStates", tracepointID.String()) + "/"
}

func getTracepointStateKey(tracepointID uuid.UUID, agentID uuid.UUID) string {
	return path.Join("/", "tracepointStates", tracepointID.String(), agentID.String())
}

func getTracepointTTLKey(tracepointID uuid.UUID) string {
	return path.Join("/", "tracepointTTL", tracepointID.String())
}

func getTracepointTTLKeys() string {
	return path.Join("/", "tracepointTTL") + "/"
}

// GetTracepointsWithNames gets which tracepoint is associated with the given name.
func (mds *KVMetadataStore) GetTracepointsWithNames(tracepointNames []string) ([]*uuid.UUID, error) {
	keys := make([]string, len(tracepointNames))
	for i, n := range tracepointNames {
		keys[i] = getTracepointWithNameKey(n)
	}

	resp, err := mds.cache.GetAll(keys)
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
func (mds *KVMetadataStore) SetTracepointWithName(tracepointName string, tracepointID uuid.UUID) error {
	tracepointIDpb := utils.ProtoFromUUID(tracepointID)
	val, err := tracepointIDpb.Marshal()
	if err != nil {
		return err
	}

	mds.cache.Set(getTracepointWithNameKey(tracepointName), string(val))
	return nil
}

// UpsertTracepoint updates or creates a new tracepoint entry in the store.
func (mds *KVMetadataStore) UpsertTracepoint(tracepointID uuid.UUID, tracepointInfo *storepb.TracepointInfo) error {
	val, err := tracepointInfo.Marshal()
	if err != nil {
		return err
	}

	mds.cache.Set(getTracepointKey(tracepointID), string(val))
	return nil
}

// DeleteTracepoint deletes the tracepoint from the store.
func (mds *KVMetadataStore) DeleteTracepoint(tracepointID uuid.UUID) error {
	mds.cache.DeleteAll([]string{getTracepointKey(tracepointID)})

	return mds.cache.DeleteWithPrefix(getTracepointStatesKey(tracepointID))
}

// GetTracepoint gets the tracepoint info from the store, if it exists.
func (mds *KVMetadataStore) GetTracepoint(tracepointID uuid.UUID) (*storepb.TracepointInfo, error) {
	resp, err := mds.cache.Get(getTracepointKey(tracepointID))
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
func (mds *KVMetadataStore) GetTracepoints() ([]*storepb.TracepointInfo, error) {
	_, vals, err := mds.cache.GetWithPrefix(getTracepointsKey())
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

// GetTracepointsForIDs gets all of the tracepoints with the given ids.
func (mds *KVMetadataStore) GetTracepointsForIDs(ids []uuid.UUID) ([]*storepb.TracepointInfo, error) {
	keys := make([]string, len(ids))
	for i, id := range ids {
		keys[i] = getTracepointKey(id)
	}

	vals, err := mds.cache.GetAll(keys)
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
func (mds *KVMetadataStore) UpdateTracepointState(state *storepb.AgentTracepointStatus) error {
	val, err := state.Marshal()
	if err != nil {
		return err
	}

	tpID := utils.UUIDFromProtoOrNil(state.ID)

	mds.cache.Set(getTracepointStateKey(tpID, utils.UUIDFromProtoOrNil(state.AgentID)), string(val))
	return nil
}

// GetTracepointStates gets all the agentTracepoint states for the given tracepoint.
func (mds *KVMetadataStore) GetTracepointStates(tracepointID uuid.UUID) ([]*storepb.AgentTracepointStatus, error) {
	_, vals, err := mds.cache.GetWithPrefix(getTracepointStatesKey(tracepointID))
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
func (mds *KVMetadataStore) SetTracepointTTL(tracepointID uuid.UUID, ttl time.Duration) error {
	return mds.cache.UncachedSetWithTTL(getTracepointTTLKey(tracepointID), "", ttl)
}

// DeleteTracepointTTLs deletes the key in the datastore for the given tracepoint TTLs.
// This is done as a single transaction, so if any deletes fail, they all fail.
func (mds *KVMetadataStore) DeleteTracepointTTLs(ids []uuid.UUID) error {
	keys := make([]string, len(ids))
	for i, id := range ids {
		keys[i] = getTracepointTTLKey(id)
	}

	return mds.cache.UncachedDeleteAll(keys)
}

// GetTracepointTTLs gets the tracepoints which still have existing TTLs.
func (mds *KVMetadataStore) GetTracepointTTLs() ([]uuid.UUID, error) {
	keys, _, err := mds.cache.GetWithPrefix(getTracepointTTLKeys())
	if err != nil {
		return nil, err
	}

	ids := make([]uuid.UUID, 0)
	for _, k := range keys {
		keyParts := strings.Split(k, "/")
		if len(keyParts) == 3 {
			id, err := uuid.FromString(keyParts[2])
			if err == nil {
				ids = append(ids, id)
			}
		}
	}

	return ids, nil
}

// WatchTracepointTTLs watches the tracepoint TTL keys for any deletions.
func (mds *KVMetadataStore) WatchTracepointTTLs() (chan uuid.UUID, chan bool) {
	deletionCh := make(chan uuid.UUID, 1000)
	ch, quitCh := mds.cache.WatchKeyEvents(getTracepointTTLKeys())
	go func() {
		defer close(deletionCh)
		for {
			select {
			case event, ok := <-ch:
				if !ok { // Channel closed.
					return
				}
				if event.EventType == kvstore.EventTypeDelete {
					// Parse the key.
					keyParts := strings.Split(event.Key, "/")
					if len(keyParts) == 3 {
						id, err := uuid.FromString(keyParts[2])
						if err == nil {
							deletionCh <- id
						}
					}
				}
			}
		}
	}()

	return deletionCh, quitCh
}
