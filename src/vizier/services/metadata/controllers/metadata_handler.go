package controllers

import (
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/runtime"

	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
)

// MetadataStore is the interface for our metadata store.
type MetadataStore interface {
	UpdateEndpoints(*metadatapb.Endpoints) error
	UpdatePod(*metadatapb.Pod) error
	UpdateService(*metadatapb.Service) error
	GetAgentsForHostnames(*[]string) (*[]string, error)
	AddToAgentUpdateQueue(string, string) error
}

// K8sMessage is a message for K8s metadata events/updates.
type K8sMessage struct {
	Object     runtime.Object
	ObjectType string
}

// MetadataHandler processes messages in its channel.
type MetadataHandler struct {
	ch  chan *K8sMessage
	mds MetadataStore
}

// NewMetadataHandler creates a new metadata handler.
func NewMetadataHandler(mds MetadataStore) (*MetadataHandler, error) {
	c := make(chan *K8sMessage)

	mh := &MetadataHandler{
		ch:  c,
		mds: mds,
	}

	go mh.MetadataListener()

	return mh, nil
}

// GetChannel returns the channel the MetadataHandler is listening to.
func (mh *MetadataHandler) GetChannel() chan *K8sMessage {
	return mh.ch
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
	hostname := []string{e.Spec.Hostname}
	agents, err := mh.mds.GetAgentsForHostnames(&hostname)

	if len(*agents) != 1 {
		log.Error("Could not get agent for hostname: " + e.Spec.Hostname)
	}

	updatePb := &messagespb.MetadataUpdateInfo_ResourceUpdate{
		Uid:  string(e.ObjectMeta.UID),
		Name: e.ObjectMeta.Name,
		Type: messagespb.POD,
	}
	update, err := updatePb.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not marshall pod update message.")
	}

	err = mh.mds.AddToAgentUpdateQueue((*agents)[0], string(update))
	if err != nil {
		log.WithError(err).Error("Could not write pod update to agent update queue.")
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
