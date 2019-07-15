package controllers

import (
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/runtime"

	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	datapb "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

const maxAgentUpdates = 10000

// MetadataStore is the interface for our metadata store.
type MetadataStore interface {
	UpdateEndpoints(*metadatapb.Endpoints) error
	UpdatePod(*metadatapb.Pod) error
	UpdateService(*metadatapb.Service) error
	UpdateContainers([]*metadatapb.ContainerInfo) error
	UpdateSchemas(uuid.UUID, []*metadatapb.SchemaInfo) error
	GetAgentsForHostnames(*[]string) (*[]string, error)
	AddToAgentUpdateQueue(string, string) error
	AddToFrontOfAgentQueue(string, *messagespb.MetadataUpdateInfo_ResourceUpdate) error
	GetFromAgentQueue(string) (*[]messagespb.MetadataUpdateInfo_ResourceUpdate, error)
	GetAgents() (*[]datapb.AgentData, error)
	GetPods() ([]*metadatapb.Pod, error)
	GetEndpoints() ([]*metadatapb.Endpoints, error)
}

// K8sMessage is a message for K8s metadata events/updResourceUpdateates.
type K8sMessage struct {
	Object     runtime.Object
	ObjectType string
}

// UpdateMessage is an update message for a specific hostname.
type UpdateMessage struct {
	Message   *messagespb.MetadataUpdateInfo_ResourceUpdate
	Hostnames *[]string
}

// MetadataHandler processes messages in its channel.
type MetadataHandler struct {
	ch            chan *K8sMessage
	mds           MetadataStore
	agentUpdateCh chan *UpdateMessage
}

// NewMetadataHandler creates a new metadata handler.
func NewMetadataHandler(mds MetadataStore) (*MetadataHandler, error) {
	c := make(chan *K8sMessage)
	agentUpdateCh := make(chan *UpdateMessage, maxAgentUpdates)

	mh := &MetadataHandler{
		ch:            c,
		mds:           mds,
		agentUpdateCh: agentUpdateCh,
	}

	go mh.MetadataListener()

	return mh, nil
}

// GetChannel returns the channel the MetadataHandler is listening to.
func (mh *MetadataHandler) GetChannel() chan *K8sMessage {
	return mh.ch
}

// ProcessAgentUpdates starts the goroutine for processing the agent update channel.
func (mh *MetadataHandler) ProcessAgentUpdates() {
	go mh.processAgentUpdates()
}

func (mh *MetadataHandler) processAgentUpdates() {
	for {
		more := mh.ProcessNextAgentUpdate()
		if !more {
			return
		}
	}
}

// ProcessNextAgentUpdate processes the next agent update in the agent update channel. Returns true if there are no more
// requests to be processed.
func (mh *MetadataHandler) ProcessNextAgentUpdate() bool {
	// If there are too many updates, something is probably very wrong.
	if len(mh.agentUpdateCh) > maxAgentUpdates {
		log.Fatal("Failed to queue maximum number of agent updates.")
	}

	msg, more := <-mh.agentUpdateCh
	if !more {
		return true
	}

	// Apply updates.
	mh.updateAgentQueues(msg.Message, msg.Hostnames)

	return false
}

// MetadataListener handles any updates on the metadata channel.
func (mh *MetadataHandler) MetadataListener() {
	for {
		msg, more := <-mh.ch
		if !more {
			return
		}

		switch msg.ObjectType {
		case "endpoints":
			mh.handleEndpointsMetadata(msg.Object)
		case "services":
			mh.handleServiceMetadata(msg.Object)
		case "pods":
			mh.handlePodMetadata(msg.Object)
		default:
			log.Error("Received unknown metadata message with type: " + msg.ObjectType)
		}
	}
}

// updateAgentQueues appends the resource update to the relevant agent queues. If appending to the queue has failed,
// it adds the update to the handler's retry channel.
func (mh *MetadataHandler) updateAgentQueues(updatePb *messagespb.MetadataUpdateInfo_ResourceUpdate, hostnames *[]string) {
	agents, err := mh.mds.GetAgentsForHostnames(hostnames)

	if agents == nil || len(*agents) == 0 {
		log.Error("Could not get any agents for hostnames.")
		mh.agentUpdateCh <- &UpdateMessage{
			Message:   updatePb,
			Hostnames: hostnames,
		}
		return
	}

	update, err := updatePb.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not marshall service update message.")
		return
	}

	var failedHostnames []string
	for i, agent := range *agents {
		err = mh.mds.AddToAgentUpdateQueue(agent, string(update))
		if err != nil {
			log.WithError(err).Error("Could not write service update to agent update queue.")
			failedHostnames = append(failedHostnames, (*hostnames)[i])
		}
	}

	if len(failedHostnames) > 0 {
		mh.agentUpdateCh <- &UpdateMessage{
			Message:   updatePb,
			Hostnames: &failedHostnames,
		}
	}
}

func (mh *MetadataHandler) handleEndpointsMetadata(o runtime.Object) {
	e := o.(*v1.Endpoints)

	// Don't record the endpoint if there is no nodename.
	if len(e.Subsets) > 0 && len(e.Subsets[0].Addresses) > 0 && e.Subsets[0].Addresses[0].NodeName == nil {
		log.Info("Received endpoint with no nodename: " + e.String())
		return
	}

	pb, err := protoutils.EndpointsToProto(e)
	if err != nil {
		log.WithError(err).Fatal("Could not convert endpoints object to protobuf.")
	}
	err = mh.mds.UpdateEndpoints(pb)
	if err != nil {
		log.WithError(err).Fatal("Could not write endpoints protobuf to metadata store.")
	}

	// Add endpoint update to agent update queues.
	var hostnames []string
	var references []*metadatapb.ObjectReference
	for _, subset := range pb.Subsets {
		for _, addr := range subset.Addresses {
			hostnames = append(hostnames, addr.NodeName)
			if addr.TargetRef != nil {
				references = append(references, addr.TargetRef)
			}
		}
	}

	updateMd := mh.getUpdateMetadata(pb.Metadata)

	updatePb := &messagespb.MetadataUpdateInfo_ResourceUpdate{
		Metadata:   updateMd,
		Type:       messagespb.SERVICE,
		References: references,
	}

	mh.agentUpdateCh <- &UpdateMessage{
		Hostnames: &hostnames,
		Message:   updatePb,
	}
}

func (mh *MetadataHandler) getUpdateMetadata(md *metadatapb.ObjectMetadata) *metadatapb.ObjectMetadata {
	return &metadatapb.ObjectMetadata{
		Name:                md.Name,
		Namespace:           md.Namespace,
		UID:                 md.UID,
		CreationTimestampNS: md.CreationTimestampNS,
		DeletionTimestampNS: md.DeletionTimestampNS,
	}
}

func (mh *MetadataHandler) handlePodMetadata(o runtime.Object) {
	e := o.(*v1.Pod)

	pb, err := protoutils.PodToProto(e)
	if err != nil {
		log.WithError(err).Fatal("Could not convert pod object to protobuf.")
	}
	err = mh.mds.UpdatePod(pb)
	if err != nil {
		log.WithError(err).Fatal("Could not write pod protobuf to metadata store.")
	}

	// Add pod update to agent update queue.
	hostname := []string{e.Spec.NodeName}

	updateMd := mh.getUpdateMetadata(pb.Metadata)

	updatePb := &messagespb.MetadataUpdateInfo_ResourceUpdate{
		Metadata: updateMd,
		Type:     messagespb.POD,
	}

	mh.agentUpdateCh <- &UpdateMessage{
		Hostnames: &hostname,
		Message:   updatePb,
	}
}

func (mh *MetadataHandler) handleServiceMetadata(o runtime.Object) {
	e := o.(*v1.Service)

	pb, err := protoutils.ServiceToProto(e)
	if err != nil {
		log.WithError(err).Fatal("Could not convert service object to protobuf.")
	}
	err = mh.mds.UpdateService(pb)
	if err != nil {
		log.WithError(err).Fatal("Could not write service protobuf to metadata store.")
	}
}
