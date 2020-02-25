package controllers_test

import (
	"fmt"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/go-nats"
	"github.com/stretchr/testify/assert"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messages "pixielabs.ai/pixielabs/src/shared/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
)

func TestMetadataTopicListener_HandleMessageWithNoRV(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)
	mockMdStore.
		EXPECT().
		GetMetadataUpdates("").
		Return([]*metadatapb.ResourceUpdate{
			&metadatapb.ResourceUpdate{ResourceVersion: "5"},
			&metadatapb.ResourceUpdate{ResourceVersion: "1"},
			&metadatapb.ResourceUpdate{ResourceVersion: "3"},
			&metadatapb.ResourceUpdate{ResourceVersion: "4"},
			&metadatapb.ResourceUpdate{ResourceVersion: "2"},
		}, nil)
	updates := make([][]byte, 0)
	// Create Metadata Service controller.
	mdl, err := controllers.NewMetadataTopicListener(mockMdStore, func(topic string, b []byte) error {
		assert.Equal(t, controllers.MetadataPublishTopic, topic)
		updates = append(updates, b)
		return nil
	})

	req := messages.MetadataUpdatesRequest{
		ResourceVersion: "",
	}
	reqPb, err := req.Marshal()
	assert.Nil(t, err)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = mdl.HandleMessage(&msg)
	assert.Nil(t, err)

	assert.Equal(t, 5, len(updates))
	for i, u := range updates {
		updatePb := &messages.MetadataUpdate{}
		proto.Unmarshal(u, updatePb)
		assert.Equal(t, fmt.Sprintf("%d", i+1), updatePb.Update.ResourceVersion)
	}
}

func TestMetadataTopicListener_HandleMessageWithRV(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)
	mockMdStore.
		EXPECT().
		GetMetadataUpdates("").
		Return([]*metadatapb.ResourceUpdate{
			&metadatapb.ResourceUpdate{ResourceVersion: "5"},
			&metadatapb.ResourceUpdate{ResourceVersion: "1"},
			&metadatapb.ResourceUpdate{ResourceVersion: "3"},
			&metadatapb.ResourceUpdate{ResourceVersion: "4"},
			&metadatapb.ResourceUpdate{ResourceVersion: "2"},
		}, nil)
	updates := make([][]byte, 0)
	// Create Metadata Service controller.
	mdl, err := controllers.NewMetadataTopicListener(mockMdStore, func(topic string, b []byte) error {
		assert.Equal(t, controllers.MetadataPublishTopic, topic)
		updates = append(updates, b)
		return nil
	})

	req := messages.MetadataUpdatesRequest{
		ResourceVersion: "3",
	}
	reqPb, err := req.Marshal()
	assert.Nil(t, err)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = mdl.HandleMessage(&msg)
	assert.Nil(t, err)

	assert.Equal(t, 3, len(updates))
	for i, u := range updates {
		updatePb := &messages.MetadataUpdate{}
		proto.Unmarshal(u, updatePb)
		assert.Equal(t, fmt.Sprintf("%d", i+3), updatePb.Update.ResourceVersion)
	}
}
