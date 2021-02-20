package controllers

import (
	uuid "github.com/satori/go.uuid"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	types "pixielabs.ai/pixielabs/src/shared/types/go"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// MetadataStore is the interface for our metadata store.
type MetadataStore interface {
	AddResourceVersion(string, *metadatapb.MetadataObject) error
	CreateAgent(agentID uuid.UUID, a *agentpb.Agent) error
	DeleteAgent(agentID uuid.UUID) error
	GetASID() (uint32, error)
	GetAgent(agentID uuid.UUID) (*agentpb.Agent, error)
	GetAgentIDForHostnamePair(hnPair *HostnameIPPair) (string, error)
	GetAgentIDFromPodName(string) (string, error)
	GetAgents() ([]*agentpb.Agent, error)
	GetAgentsDataInfo() (map[uuid.UUID]*messagespb.AgentDataInfo, error)
	GetAgentDataInfo(agentID uuid.UUID) (*messagespb.AgentDataInfo, error)
	GetAgentsForHostnamePairs(*[]*HostnameIPPair) ([]string, error)
	GetComputedSchema() (*storepb.ComputedSchema, error)
	GetContainers() ([]*metadatapb.ContainerInfo, error)
	GetEndpoints() ([]*metadatapb.Endpoints, error)
	GetHostnameIPPairFromPodName(string, string) (*HostnameIPPair, error)
	GetKelvinIDs() ([]string, error)
	GetNamespaces() ([]*metadatapb.Namespace, error)
	GetNodeEndpoints(hostname *HostnameIPPair) ([]*metadatapb.Endpoints, error)
	GetNodePods(hostname *HostnameIPPair) ([]*metadatapb.Pod, error)
	GetNodes() ([]*metadatapb.Node, error)
	GetPodCIDRs() []string
	GetPods() ([]*metadatapb.Pod, error)
	GetProcesses(upids []*types.UInt128) ([]*metadatapb.ProcessInfo, error)
	GetServiceCIDR() string
	GetServices() ([]*metadatapb.Service, error)
	GetSubscriberResourceVersion(string) (string, error)
	UpdateAgent(agentID uuid.UUID, a *agentpb.Agent) error
	UpdateAgentDataInfo(agentID uuid.UUID, dataInfo *messagespb.AgentDataInfo) error
	UpdateContainer(*metadatapb.ContainerInfo) error
	UpdateContainersFromPod(*metadatapb.Pod, bool) error
	UpdateEndpoints(*metadatapb.Endpoints, bool) error
	UpdateNamespace(*metadatapb.Namespace, bool) error
	UpdateNode(*metadatapb.Node, bool) error
	UpdatePod(*metadatapb.Pod, bool) error
	UpdateProcesses(processes []*metadatapb.ProcessInfo) error
	UpdateSchemas(agentID uuid.UUID, schemas []*storepb.TableInfo) error
	UpdateService(*metadatapb.Service, bool) error
	UpdateSubscriberResourceVersion(string, string) error
}
