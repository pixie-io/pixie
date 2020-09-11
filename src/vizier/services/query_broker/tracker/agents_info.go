package tracker

import (
	"fmt"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

// KelvinSSLTargetOverride the hostname used for SSL target override when sending data to Kelvin.
// TODO(nserrino): This value may differ in the future.
// Have this come from the Kelvins themselves when they send their updated state.
const KelvinSSLTargetOverride = "kelvin.pl.svc"

// AgentsInfo tracks information about the distributed state of the system.
type AgentsInfo interface {
	ClearState()
	UpdateAgentsInfo(agentUpdates []*metadatapb.AgentUpdate, schemaInfos []*distributedpb.SchemaInfo, schemasUpdated bool) error
	DistributedState() *distributedpb.DistributedState
}

// AgentsInfoImpl implements AgentsInfo to track information about the distributed state of the system.
type AgentsInfoImpl struct {
	ds *distributedpb.DistributedState
}

// NewAgentsInfo creates an empty agents info.
func NewAgentsInfo() AgentsInfo {
	return &AgentsInfoImpl{
		&distributedpb.DistributedState{
			SchemaInfo: []*distributedpb.SchemaInfo{},
			CarnotInfo: []*distributedpb.CarnotInfo{},
		},
	}
}

// NewTestAgentsInfo creates an agents info from a passed in distributed state.
func NewTestAgentsInfo(ds *distributedpb.DistributedState) AgentsInfo {
	return &AgentsInfoImpl{
		ds,
	}
}

// ClearState clears the agents info state.
func (a *AgentsInfoImpl) ClearState() {
	log.Infof("Clearing distributed state")
	a.ds = &distributedpb.DistributedState{
		SchemaInfo: []*distributedpb.SchemaInfo{},
		CarnotInfo: []*distributedpb.CarnotInfo{},
	}
}

// UpdateAgentsInfo creates a new agent info.
// This function must be resilient to receiving the same update twice for a given agent.
func (a *AgentsInfoImpl) UpdateAgentsInfo(agentUpdates []*metadatapb.AgentUpdate, schemaInfos []*distributedpb.SchemaInfo,
	schemasUpdated bool) error {
	if schemasUpdated {
		log.Infof("Updating schemas to %s tables", len(schemaInfos))
		a.ds.SchemaInfo = schemaInfos
	}

	carnotInfoMap := make(map[uuid.UUID]*distributedpb.CarnotInfo)
	for _, carnotInfo := range a.ds.CarnotInfo {
		agentUUID, err := utils.UUIDFromProto(carnotInfo.AgentID)
		if err != nil {
			return err
		}
		carnotInfoMap[agentUUID] = carnotInfo
	}

	log.Infof("%d agents present in tracker before update", len(carnotInfoMap))

	createdAgents := 0
	deletedAgents := 0
	updatedAgents := 0
	updatedAgentsDataInfo := 0

	for _, agentUpdate := range agentUpdates {
		agentUUID, err := utils.UUIDFromProto(agentUpdate.AgentID)
		if err != nil {
			return err
		}
		// case 1: agent info update
		agent := agentUpdate.GetAgent()
		if agent != nil {
			if _, present := carnotInfoMap[agentUUID]; present {
				updatedAgents++
			} else {
				createdAgents++
			}

			if agent.Info.Capabilities == nil || agent.Info.Capabilities.CollectsData {
				var metadataInfo *distributedpb.MetadataInfo
				if carnotInfo, present := carnotInfoMap[agentUUID]; present {
					metadataInfo = carnotInfo.MetadataInfo
				}
				// this is a PEM
				carnotInfoMap[agentUUID] = makeAgentCarnotInfo(agentUUID, agent.ASID, metadataInfo)
			} else {
				// this is a Kelvin
				kelvinGRPCAddress := agent.Info.IPAddress
				carnotInfoMap[agentUUID] = makeKelvinCarnotInfo(agentUUID, kelvinGRPCAddress, agent.ASID)
			}
		}
		// case 2: agent data info update
		dataInfo := agentUpdate.GetDataInfo()
		if dataInfo != nil {
			updatedAgentsDataInfo++
			carnotInfo, present := carnotInfoMap[agentUUID]
			if !present {
				return fmt.Errorf("Could not update agent table metadata of unknown agent %s", agentUUID.String())
			}
			if carnotInfo == nil {
				return fmt.Errorf("Carnot info is nil for agent %s, but received agent data info", agentUUID.String())
			}
			if dataInfo.MetadataInfo != nil {
				carnotInfo.MetadataInfo = dataInfo.MetadataInfo
			}
		}
		// case 3: agent deleted
		if agentUpdate.GetDeleted() {
			deletedAgents++
			delete(carnotInfoMap, agentUUID)
		}
	}

	log.Infof("Created %s agents, deleted %s agents, updated %s agents, updated %s agents data info",
		createdAgents, deletedAgents, updatedAgents, updatedAgentsDataInfo)
	log.Infof("%d agents present in tracker after update", len(carnotInfoMap))

	// reset the array and recreate.
	a.ds.CarnotInfo = []*distributedpb.CarnotInfo{}
	for _, carnotInfo := range carnotInfoMap {
		a.ds.CarnotInfo = append(a.ds.CarnotInfo, carnotInfo)
	}
	return nil
}

// DistributedState returns the current distributed state. Will return nil if not existent.
func (a *AgentsInfoImpl) DistributedState() *distributedpb.DistributedState {
	return a.ds
}

func makeAgentCarnotInfo(agentID uuid.UUID, asid uint32, agentMetadata *distributedpb.MetadataInfo) *distributedpb.CarnotInfo {
	return &distributedpb.CarnotInfo{
		QueryBrokerAddress:   agentID.String(),
		AgentID:              utils.ProtoFromUUID(&agentID),
		ASID:                 asid,
		HasGRPCServer:        false,
		HasDataStore:         true,
		ProcessesData:        true,
		AcceptsRemoteSources: false,
		MetadataInfo:         agentMetadata,
	}
}

func makeKelvinCarnotInfo(agentID uuid.UUID, grpcAddress string, asid uint32) *distributedpb.CarnotInfo {
	return &distributedpb.CarnotInfo{
		QueryBrokerAddress:   agentID.String(),
		AgentID:              utils.ProtoFromUUID(&agentID),
		ASID:                 asid,
		HasGRPCServer:        true,
		GRPCAddress:          grpcAddress,
		HasDataStore:         false,
		ProcessesData:        true,
		AcceptsRemoteSources: true,
		// When we support persistent storage, Kelvins will also have MetadataInfo.
		MetadataInfo: nil,
		// TODO(nserrino): When this value is no longer a constant, we will need to get the value from
		// the agent update.
		SSLTargetName: KelvinSSLTargetOverride,
	}
}
