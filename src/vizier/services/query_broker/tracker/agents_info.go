package tracker

import (
	uuid "github.com/satori/go.uuid"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
)

// AgentsInfo tracks information about the distributed state of the system.
type AgentsInfo struct {
	ds     *distributedpb.DistributedState
	schema *schemapb.Schema
}

// NewAgentsInfo creates a new agent info.
func NewAgentsInfo(schema *schemapb.Schema, agentInfos *metadatapb.AgentInfoResponse, agentTableMetadataResp *metadatapb.AgentTableMetadataResponse) (*AgentsInfo, error) {
	var agentTableMetadata = make(map[uuid.UUID]*distributedpb.MetadataInfo)
	var schemaInfoMap = make(map[string]*distributedpb.SchemaInfo)
	for _, md := range agentTableMetadataResp.MetadataByAgent {
		if md.DataInfo != nil && md.DataInfo.MetadataInfo != nil {
			agentUUID, err := utils.UUIDFromProto(md.AgentID)
			if err != nil {
				return nil, err
			}
			agentTableMetadata[agentUUID] = md.DataInfo.MetadataInfo
			// Add the Schema.
			if md.Schema == nil {
				continue
			}
			for tableName, relation := range md.Schema.RelationMap {
				if elm, ok := schemaInfoMap[tableName]; ok {
					// TODO(philkuz) should we check that the schema is the same?
					elm.AgentList = append(elm.AgentList, md.AgentID)
					schemaInfoMap[tableName] = elm
				} else {
					schemaInfoMap[tableName] = &distributedpb.SchemaInfo{
						Name:      tableName,
						Relation:  relation,
						AgentList: []*uuidpb.UUID{md.AgentID},
					}
				}
			}

		}
	}

	var kelvinList []*agentpb.Agent
	var pemList []*agentpb.Agent

	for _, m := range agentInfos.Info {
		if m.Agent.Info.Capabilities == nil || m.Agent.Info.Capabilities.CollectsData {
			pemList = append(pemList, m.Agent)
		} else {
			kelvinList = append(kelvinList, m.Agent)
		}
	}
	carnotInfoList := make([]*distributedpb.CarnotInfo, 0)
	for _, pem := range pemList {
		pemID := utils.UUIDFromProtoOrNil(pem.Info.AgentID)
		var agentMetadata *distributedpb.MetadataInfo
		if md, found := agentTableMetadata[pemID]; found {
			agentMetadata = md
		}
		carnotInfoList = append(carnotInfoList, makeAgentCarnotInfo(pemID, pem.ASID, agentMetadata))
	}

	for _, kelvin := range kelvinList {
		kelvinID := utils.UUIDFromProtoOrNil(kelvin.Info.AgentID)
		kelvinGRPCAddress := kelvin.Info.IPAddress
		carnotInfoList = append(carnotInfoList, makeKelvinCarnotInfo(kelvinID, kelvinGRPCAddress, kelvin.ASID))
	}

	schemaInfoArray := make([]*distributedpb.SchemaInfo, 0)
	for _, schemaInfo := range schemaInfoMap {
		schemaInfoArray = append(schemaInfoArray, schemaInfo)
	}

	return &AgentsInfo{
		&distributedpb.DistributedState{
			CarnotInfo: carnotInfoList,
			SchemaInfo: schemaInfoArray,
		},
		schema,
	}, nil
}

// DistributedState returns the current distributed state. Will return nil if not existent.
func (a AgentsInfo) DistributedState() *distributedpb.DistributedState {
	return a.ds
}

// Schema returns the current Schema. Returns nil if not existent.
func (a AgentsInfo) Schema() *schemapb.Schema {
	return a.schema
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
	}
}
