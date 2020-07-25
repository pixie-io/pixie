package controllers

import (
	"bytes"
	"errors"

	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	logicalpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir/logical"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

var (
	// ErrTracepointAlreadyExists is produced if a tracepoint already exists with the given name
	// and does not have a matching schema.
	ErrTracepointAlreadyExists = errors.New("Tracepoint already exists")
)

// TracepointStore is a datastore which can store, update, and retrieve information about tracepoints.
type TracepointStore interface {
	UpsertTracepoint(uuid.UUID, *storepb.TracepointInfo) error
	GetTracepoint(uuid.UUID) (*storepb.TracepointInfo, error)
	GetTracepoints() ([]*storepb.TracepointInfo, error)
	UpdateTracepointState(*storepb.AgentTracepointStatus) error
	GetTracepointStates(uuid.UUID) ([]*storepb.AgentTracepointStatus, error)
	SetTracepointWithName(string, uuid.UUID) error
	GetTracepointWithName(string) (*uuid.UUID, error)
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

func comparePrograms(p1 *logicalpb.Program, p2 *logicalpb.Program) bool {
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

// CreateTracepoint creates and stores info about the given tracepoint.
func (m *TracepointManager) CreateTracepoint(tracepointName string, program *logicalpb.Program) (*uuid.UUID, error) {
	// Check to see if a tracepoint with the matching name already exists.
	prevTracepointID, err := m.mds.GetTracepointWithName(tracepointName)
	if err != nil {
		return nil, err
	}

	if prevTracepointID != nil { // Existing tracepoint already exists.
		prevTracepoint, err := m.mds.GetTracepoint(*prevTracepointID)
		if err == nil && prevTracepoint != nil {

			//  We can replace it if the outputs are the same.
			if len(prevTracepoint.Program.Outputs) != len(program.Outputs) {
				return prevTracepointID, ErrTracepointAlreadyExists
			}
			for i, output := range prevTracepoint.Program.Outputs {
				if len(output.Fields) != len(program.Outputs[i].Fields) {
					return prevTracepointID, ErrTracepointAlreadyExists
				}
				for j, field := range output.Fields {
					if field != program.Outputs[i].Fields[j] {
						return prevTracepointID, ErrTracepointAlreadyExists
					}
				}
			}
			// Check if the tracepoints are exactly the same.
			if comparePrograms(program, prevTracepoint.Program) {
				return prevTracepointID, ErrTracepointAlreadyExists
			}

			// We should delete the old tracepoint.
			// TODO(michelle): We should also send termination requests to the PEMS.
			// To do this, we can set an expired TTL, when that logic is setup.
			prevTracepoint.ExpectedState = statuspb.TERMINATED_STATE
			err = m.mds.UpsertTracepoint(*prevTracepointID, prevTracepoint)
			if err != nil {
				return nil, err
			}
		}
	}

	tpID := uuid.NewV4()
	newTracepoint := &storepb.TracepointInfo{
		TracepointID:   utils.ProtoFromUUID(&tpID),
		Program:        program,
		TracepointName: tracepointName,
		ExpectedState:  statuspb.RUNNING_STATE,
	}
	err = m.mds.UpsertTracepoint(tpID, newTracepoint)
	if err != nil {
		return nil, err
	}
	err = m.mds.SetTracepointWithName(tracepointName, tpID)
	return &tpID, nil
}

// GetAllTracepoints gets all the tracepoints currently tracked by the metadata service.
func (m *TracepointManager) GetAllTracepoints() ([]*storepb.TracepointInfo, error) {
	return m.mds.GetTracepoints()
}

// UpdateAgentTracepointStatus updates the tracepoint info with the new agent tracepoint status.
func (m *TracepointManager) UpdateAgentTracepointStatus(tracepointID *uuidpb.UUID, agentID *uuidpb.UUID, state statuspb.LifeCycleState, status *statuspb.Status) error {
	tracepointState := &storepb.AgentTracepointStatus{
		State:        state,
		Status:       status,
		TracepointID: tracepointID,
		AgentID:      agentID,
	}

	return m.mds.UpdateTracepointState(tracepointState)
}

// RegisterTracepoint sends requests to the given agents to register the specified tracepoint.
func (m *TracepointManager) RegisterTracepoint(agentIDs []uuid.UUID, tracepointID uuid.UUID, program *logicalpb.Program) error {
	tracepointReq := messages.VizierMessage{
		Msg: &messages.VizierMessage_TracepointMessage{
			TracepointMessage: &messages.TracepointMessage{
				Msg: &messages.TracepointMessage_RegisterTracepointRequest{
					RegisterTracepointRequest: &messages.RegisterTracepointRequest{
						Program:      program,
						TracepointID: utils.ProtoFromUUID(&tracepointID),
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
