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
	// ErrProbeAlreadyExists is produced if a probe already exists with the given name
	// and does not have a matching schema.
	ErrProbeAlreadyExists = errors.New("Probe already exists")
)

// ProbeStore is a datastore which can store, update, and retrieve information about probes.
type ProbeStore interface {
	UpsertProbe(string, *storepb.ProbeInfo) error
	GetProbe(string) (*storepb.ProbeInfo, error)
	GetProbes() ([]*storepb.ProbeInfo, error)
	UpdateProbeState(*storepb.AgentProbeStatus) error
	GetProbeStates(string) ([]*storepb.AgentProbeStatus, error)
}

// ProbeManager manages the probes deployed in the cluster.
type ProbeManager struct {
	conn *nats.Conn
	mds  ProbeStore
}

// NewProbeManager creates a new probe manager.
func NewProbeManager(conn *nats.Conn, mds ProbeStore) *ProbeManager {
	return &ProbeManager{
		conn, mds,
	}
}

// CreateProbe creates and stores info about the given probe.
func (m *ProbeManager) CreateProbe(probeName string, program *logicalpb.Program) (string, error) {
	// Check to see if a probe with the matching name already exists.
	prevProbe, err := m.mds.GetProbe(probeName)
	if err != nil {
		return "", err
	}

	if prevProbe != nil { // Existing probe already exists. We can replace it if the outputs are the same.
		if len(prevProbe.Program.Outputs) != len(program.Outputs) {
			return "", ErrProbeAlreadyExists
		}
		for i, output := range prevProbe.Program.Outputs {
			if len(output.Fields) != len(program.Outputs[i].Fields) {
				return "", ErrProbeAlreadyExists
			}
			for j, field := range output.Fields {
				if field != program.Outputs[i].Fields[j] {
					return "", ErrProbeAlreadyExists
				}
			}
		}
	}

	newProbe := &storepb.ProbeInfo{
		ProbeID: probeName,
		Program: program,
	}
	err = m.mds.UpsertProbe(probeName, newProbe)
	if err != nil {
		return "", err
	}
	return probeName, nil
}

// GetAllProbes gets all the probes currently tracked by the metadata service.
func (m *ProbeManager) GetAllProbes() ([]*storepb.ProbeInfo, error) {
	return m.mds.GetProbes()
}

// UpdateAgentProbeStatus updates the probe info with the new agent probe status.
func (m *ProbeManager) UpdateAgentProbeStatus(probeID string, agentID *uuidpb.UUID, state statuspb.LifeCycleState, status *statuspb.Status) error {
	probeState := &storepb.AgentProbeStatus{
		State:   state,
		Status:  status,
		ProbeID: probeID,
		AgentID: agentID,
	}

	return m.mds.UpdateProbeState(probeState)
}

// RegisterProbe sends requests to the given agents to register the specified probe.
func (m *ProbeManager) RegisterProbe(agentIDs []uuid.UUID, probeID string, program *logicalpb.Program) error {
	probeReq := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterProbeRequest{
			RegisterProbeRequest: &messages.RegisterProbeRequest{
				Program: program,
				ProbeID: probeID,
			},
		},
	}
	msg, err := probeReq.Marshal()
	if err != nil {
		return err
	}

	// Send request to all agents.
	for _, agentID := range agentIDs {
		topic := GetAgentTopicFromUUID(agentID)

		err := m.conn.Publish(topic, msg)
		if err != nil {
			log.WithError(err).Error("Could not send ProbeRegisterRequest.")
			return err
		}
	}

	return nil
}

// GetProbeInfo gets the status for the probe with the given ID.
func (m *ProbeManager) GetProbeInfo(probeID string) (*storepb.ProbeInfo, error) {
	return m.mds.GetProbe(probeID)
}

// GetProbeStates gets all the known agent states for the given probe.
func (m *ProbeManager) GetProbeStates(probeID string) ([]*storepb.AgentProbeStatus, error) {
	return m.mds.GetProbeStates(probeID)
}
