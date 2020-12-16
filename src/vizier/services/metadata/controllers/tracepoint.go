package controllers

import (
	"bytes"
	"errors"
	"fmt"
	"time"

	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	logicalpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir/logicalpb"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

var (
	// ErrTracepointAlreadyExists is produced if a tracepoint already exists with the given name
	// and does not have a matching schema.
	ErrTracepointAlreadyExists = errors.New("TracepointDeployment already exists")
)

// TracepointStore is a datastore which can store, update, and retrieve information about tracepoints.
type TracepointStore interface {
	UpsertTracepoint(uuid.UUID, *storepb.TracepointInfo) error
	GetTracepoint(uuid.UUID) (*storepb.TracepointInfo, error)
	GetTracepoints() ([]*storepb.TracepointInfo, error)
	UpdateTracepointState(*storepb.AgentTracepointStatus) error
	GetTracepointStates(uuid.UUID) ([]*storepb.AgentTracepointStatus, error)
	SetTracepointWithName(string, uuid.UUID) error
	GetTracepointsWithNames([]string) ([]*uuid.UUID, error)
	GetTracepointsForIDs([]uuid.UUID) ([]*storepb.TracepointInfo, error)
	SetTracepointTTL(uuid.UUID, time.Duration) error
	DeleteTracepointTTLs([]uuid.UUID) error
	WatchTracepointTTLs() (chan uuid.UUID, chan bool)
	GetAgents() ([]*agentpb.Agent, error)
	DeleteTracepoint(uuid.UUID) error
	GetTracepointTTLs() ([]uuid.UUID, error)
}

// TracepointManager manages the tracepoints deployed in the cluster.
type TracepointManager struct {
	conn *nats.Conn
	mds  TracepointStore
}

// NewTracepointManager creates a new tracepoint manager.
func NewTracepointManager(conn *nats.Conn, mds TracepointStore) *TracepointManager {
	return &TracepointManager{
		conn, mds,
	}
}

// SyncTracepoints deletes any tracpeoints that should have been terminated when MDS was not running.
func (m *TracepointManager) SyncTracepoints() error {
	tps, err := m.mds.GetTracepoints()
	if err != nil {
		return err
	}

	ttls, err := m.mds.GetTracepointTTLs()
	if err != nil {
		return err
	}

	// Make a map so we can determine which tracepoints don't have a TTL.
	tpMap := make(map[uuid.UUID]bool)
	for _, tp := range tps {
		tpMap[utils.UUIDFromProtoOrNil(tp.ID)] = false
	}

	for _, tp := range ttls {
		tpMap[tp] = true
	}

	for k, v := range tpMap {
		if v == false {
			err = m.deleteTracepoint(k)
			if err != nil {
				log.WithField("id", k).WithError(err).Error("Error deleting tracepoint during sync")
			}
		}
	}

	return nil
}

func compareTracepoints(p1 *logicalpb.TracepointDeployment_Tracepoint, p2 *logicalpb.TracepointDeployment_Tracepoint) bool {
	val1, err := p1.Marshal()
	if err != nil {
		return false
	}
	val2, err := p2.Marshal()
	if err != nil {
		return false
	}
	return bytes.Equal(val1, val2)
}

// WatchTTLs watches the tracepoint TTLs and terminates the associated tracepoint.
func (m *TracepointManager) WatchTTLs(watcherQuitCh chan bool) {
	deletedTracepoints, quitCh := m.mds.WatchTracepointTTLs()
	defer func() { quitCh <- true }()
	for {
		select {
		case <-watcherQuitCh:
			return
		case tpID := <-deletedTracepoints:
			err := m.terminateTracepoint(tpID)
			if err != nil {
				log.WithError(err).Error("Could not delete tracepoint")
			}
		}
	}
}

func (m *TracepointManager) terminateTracepoint(id uuid.UUID) error {
	// Update state in datastore to terminated.
	tp, err := m.mds.GetTracepoint(id)
	if err != nil {
		return err
	}

	tp.ExpectedState = statuspb.TERMINATED_STATE
	err = m.mds.UpsertTracepoint(id, tp)
	if err != nil {
		return err
	}

	// Send termination messages to PEMs.
	tracepointReq := messages.VizierMessage{
		Msg: &messages.VizierMessage_TracepointMessage{
			TracepointMessage: &messages.TracepointMessage{
				Msg: &messages.TracepointMessage_RemoveTracepointRequest{
					RemoveTracepointRequest: &messages.RemoveTracepointRequest{
						ID: utils.ProtoFromUUID(id),
					},
				},
			},
		},
	}
	msg, err := tracepointReq.Marshal()
	if err != nil {
		return err
	}

	agents, err := m.mds.GetAgents()
	if err != nil {
		return err
	}

	// Send request to all agents.
	for _, a := range agents {
		topic := GetAgentTopicFromUUID(utils.UUIDFromProtoOrNil(a.Info.AgentID))

		err := m.conn.Publish(topic, msg)
		if err != nil {
			log.WithError(err).Error("Could not send RemoveTracepointRequest.")
			return err
		}
	}

	return nil
}

func (m *TracepointManager) deleteTracepoint(id uuid.UUID) error {
	return m.mds.DeleteTracepoint(id)
}

// CreateTracepoint creates and stores info about the given tracepoint.
func (m *TracepointManager) CreateTracepoint(tracepointName string, tracepointDeployment *logicalpb.TracepointDeployment, ttl time.Duration) (*uuid.UUID, error) {
	// Check to see if a tracepoint with the matching name already exists.
	resp, err := m.mds.GetTracepointsWithNames([]string{tracepointName})
	if err != nil {
		return nil, err
	}

	if len(resp) != 1 {
		return nil, errors.New("Could not fetch tracepoint")
	}
	prevTracepointID := resp[0]

	if prevTracepointID != nil { // Existing tracepoint already exists.
		prevTracepoint, err := m.mds.GetTracepoint(*prevTracepointID)
		if err != nil {
			return nil, err
		}
		if prevTracepoint != nil && prevTracepoint.ExpectedState != statuspb.TERMINATED_STATE {
			// If everything is exactly the same, no need to redeploy
			//   - return prevTracepointID, ErrTracepointAlreadyExists
			// If anything inside tracepoints has changed
			//   - delete old tracepoints, and insert new tracepoints.

			// Check if the tracepoints are exactly the same.
			allTpsSame := true

			if len(prevTracepoint.Tracepoint.Tracepoints) == len(tracepointDeployment.Tracepoints) {
				for i := range prevTracepoint.Tracepoint.Tracepoints {
					if tracepointDeployment.Tracepoints[i] != nil {
						if !compareTracepoints(tracepointDeployment.Tracepoints[i], prevTracepoint.Tracepoint.Tracepoints[i]) {
							allTpsSame = false
							break
						}
					}
				}
			} else {
				allTpsSame = false
			}

			if allTpsSame {
				err = m.mds.SetTracepointTTL(*prevTracepointID, ttl)
				return prevTracepointID, ErrTracepointAlreadyExists
			}

			// Something has changed, so trigger termination of the old tracepoint.
			err = m.mds.DeleteTracepointTTLs([]uuid.UUID{*prevTracepointID})
			if err != nil {
				return nil, err
			}
		}
	}

	tpID := uuid.NewV4()
	newTracepoint := &storepb.TracepointInfo{
		ID:            utils.ProtoFromUUID(tpID),
		Tracepoint:    tracepointDeployment,
		Name:          tracepointName,
		ExpectedState: statuspb.RUNNING_STATE,
	}
	err = m.mds.UpsertTracepoint(tpID, newTracepoint)
	if err != nil {
		return nil, err
	}
	err = m.mds.SetTracepointTTL(tpID, ttl)
	err = m.mds.SetTracepointWithName(tracepointName, tpID)
	return &tpID, nil
}

// GetAllTracepoints gets all the tracepoints currently tracked by the metadata service.
func (m *TracepointManager) GetAllTracepoints() ([]*storepb.TracepointInfo, error) {
	return m.mds.GetTracepoints()
}

// UpdateAgentTracepointStatus updates the tracepoint info with the new agent tracepoint status.
func (m *TracepointManager) UpdateAgentTracepointStatus(tracepointID *uuidpb.UUID, agentID *uuidpb.UUID, state statuspb.LifeCycleState, status *statuspb.Status) error {
	if state == statuspb.TERMINATED_STATE { // If all agent tracepoint statuses are now terminated, we can finally delete the tracepoint from the datastore.
		tID := utils.UUIDFromProtoOrNil(tracepointID)
		states, err := m.GetTracepointStates(tID)
		if err != nil {
			return err
		}
		allTerminated := true
		for _, s := range states {
			if s.State != statuspb.TERMINATED_STATE && !s.AgentID.Equal(agentID) {
				allTerminated = false
				break
			}
		}

		if allTerminated {
			return m.deleteTracepoint(tID)
		}
	}

	tracepointState := &storepb.AgentTracepointStatus{
		State:   state,
		Status:  status,
		ID:      tracepointID,
		AgentID: agentID,
	}

	return m.mds.UpdateTracepointState(tracepointState)
}

// RegisterTracepoint sends requests to the given agents to register the specified tracepoint.
func (m *TracepointManager) RegisterTracepoint(agentIDs []uuid.UUID, tracepointID uuid.UUID, tracepointDeployment *logicalpb.TracepointDeployment) error {
	tracepointReq := messages.VizierMessage{
		Msg: &messages.VizierMessage_TracepointMessage{
			TracepointMessage: &messages.TracepointMessage{
				Msg: &messages.TracepointMessage_RegisterTracepointRequest{
					RegisterTracepointRequest: &messages.RegisterTracepointRequest{
						TracepointDeployment: tracepointDeployment,
						ID:                   utils.ProtoFromUUID(tracepointID),
					},
				},
			},
		},
	}
	msg, err := tracepointReq.Marshal()
	if err != nil {
		return err
	}

	// Send request to all agents.
	for _, agentID := range agentIDs {
		topic := GetAgentTopicFromUUID(agentID)

		err := m.conn.Publish(topic, msg)
		if err != nil {
			log.WithError(err).Error("Could not send TracepointRegisterRequest.")
			return err
		}
	}

	return nil
}

// GetTracepointInfo gets the status for the tracepoint with the given ID.
func (m *TracepointManager) GetTracepointInfo(tracepointID uuid.UUID) (*storepb.TracepointInfo, error) {
	return m.mds.GetTracepoint(tracepointID)
}

// GetTracepointStates gets all the known agent states for the given tracepoint.
func (m *TracepointManager) GetTracepointStates(tracepointID uuid.UUID) ([]*storepb.AgentTracepointStatus, error) {
	return m.mds.GetTracepointStates(tracepointID)
}

// GetTracepointsForIDs gets all the tracepoint infos for the given ids.
func (m *TracepointManager) GetTracepointsForIDs(ids []uuid.UUID) ([]*storepb.TracepointInfo, error) {
	return m.mds.GetTracepointsForIDs(ids)
}

// RemoveTracepoints starts the termination process for the tracepoints with the given names.
func (m *TracepointManager) RemoveTracepoints(names []string) error {
	tpIDs, err := m.mds.GetTracepointsWithNames(names)
	if err != nil {
		return err
	}

	ids := make([]uuid.UUID, len(tpIDs))

	for i, id := range tpIDs {
		if id == nil {
			return fmt.Errorf("Could not find tracepoint for given name: %s", names[i])
		}
		ids[i] = *id
	}

	return m.mds.DeleteTracepointTTLs(ids)
}
