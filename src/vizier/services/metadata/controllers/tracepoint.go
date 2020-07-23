package controllers

import (
	"errors"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	logicalpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir"
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
	UpsertTracepoint(string, *storepb.TracepointInfo) error
	GetTracepoint(string) (*storepb.TracepointInfo, error)
	GetTracepoints() ([]*storepb.TracepointInfo, error)
	UpdateTracepointState(*storepb.AgentTracepointStatus) error
	GetTracepointStates(string) ([]*storepb.AgentTracepointStatus, error)
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

// CreateTracepoint creates and stores info about the given tracepoint.
func (m *TracepointManager) CreateTracepoint(tracepointName string, program *logicalpb.Program) (string, error) {
	// Check to see if a tracepoint with the matching name already exists.
	prevTracepoint, err := m.mds.GetTracepoint(tracepointName)
	if err != nil {
		return "", err
	}

	if prevTracepoint != nil { // Existing tracepoint already exists. We can replace it if the outputs are the same.
		if len(prevTracepoint.Program.Outputs) != len(program.Outputs) {
			return "", ErrTracepointAlreadyExists
		}
		for i, output := range prevTracepoint.Program.Outputs {
			if len(output.Fields) != len(program.Outputs[i].Fields) {
				return "", ErrTracepointAlreadyExists
			}
			for j, field := range output.Fields {
				if field != program.Outputs[i].Fields[j] {
					return "", ErrTracepointAlreadyExists
				}
			}
		}
	}

	newTracepoint := &storepb.TracepointInfo{
		TracepointID: tracepointName,
		Program:      program,
	}
	err = m.mds.UpsertTracepoint(tracepointName, newTracepoint)
	if err != nil {
		return "", err
	}
	return tracepointName, nil
}

// GetAllTracepoints gets all the tracepoints currently tracked by the metadata service.
func (m *TracepointManager) GetAllTracepoints() ([]*storepb.TracepointInfo, error) {
	return m.mds.GetTracepoints()
}

// UpdateAgentTracepointStatus updates the tracepoint info with the new agent tracepoint status.
func (m *TracepointManager) UpdateAgentTracepointStatus(tracepointID string, agentID *uuidpb.UUID, state statuspb.LifeCycleState, status *statuspb.Status) error {
	tracepointState := &storepb.AgentTracepointStatus{
		State:        state,
		Status:       status,
		TracepointID: tracepointID,
		AgentID:      agentID,
	}

	return m.mds.UpdateTracepointState(tracepointState)
}

// RegisterTracepoint sends requests to the given agents to register the specified tracepoint.
func (m *TracepointManager) RegisterTracepoint(agentIDs []uuid.UUID, tracepointID string, program *logicalpb.Program) error {
	tracepointReq := messages.VizierMessage{
		Msg: &messages.VizierMessage_TracepointMessage{
			TracepointMessage: &messages.TracepointMessage{
				Msg: &messages.TracepointMessage_RegisterTracepointRequest{
					RegisterTracepointRequest: &messages.RegisterTracepointRequest{
						Program:      program,
						TracepointID: tracepointID,
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
func (m *TracepointManager) GetTracepointInfo(tracepointID string) (*storepb.TracepointInfo, error) {
	return m.mds.GetTracepoint(tracepointID)
}

// GetTracepointStates gets all the known agent states for the given tracepoint.
func (m *TracepointManager) GetTracepointStates(tracepointID string) ([]*storepb.AgentTracepointStatus, error) {
	return m.mds.GetTracepointStates(tracepointID)
}
