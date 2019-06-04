package controllers

import (
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/runtime"

	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
)

// MetadataStore is the interface for our metadata store.
type MetadataStore interface {
	UpdateEndpoints(*metadatapb.Endpoints) error
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
		default:
			log.Error("Received unknown metadata message with type: " + msg.ObjectType)
		}
	}
}

func (mh *MetadataHandler) handleEndpointsMetadata(o runtime.Object) {
	e := o.(*v1.Endpoints)

	pb, err := protoutils.EndpointsToProto(e)
	if err != nil {
		log.Info("Could not convert endpoints object to protobuf.")
	}
	err = mh.mds.UpdateEndpoints(pb)
	if err != nil {
		log.Info("Could not write endpoints protobuf to metadata store.")
	}
}
