package controllers_test

import (
	"fmt"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
)

func TestMetadataTopicListener_MetadataSubscriber(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)
	mockMdStore.
		EXPECT().
		UpdatePod(gomock.Any(), false).
		Return(nil)
	mockMdStore.
		EXPECT().
		UpdateContainersFromPod(gomock.Any(), false).
		Return(nil)

	isLeader := true
	mdh, _ := controllers.NewMetadataHandler(mockMdStore, &isLeader)

	updates := make([][]byte, 0)
	// Create Metadata Service controller.
	_, _ = controllers.NewMetadataTopicListener(mockMdStore, mdh, func(topic string, b []byte) error {
		assert.Equal(t, controllers.MetadataUpdatesTopic, topic)
		updates = append(updates, b)
		return nil
	})

	// Send update to metadata handler and check that the update is sent to the metadata topic listener.
	expectedPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	// Create pod object.
	ownerRefs := make([]metav1.OwnerReference, 1)
	ownerRefs[0] = metav1.OwnerReference{
		Kind: "pod",
		Name: "test",
		UID:  "abcd",
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	metadata := metav1.ObjectMeta{
		Name:              "object_md",
		UID:               "ijkl",
		ResourceVersion:   "1",
		ClusterName:       "a_cluster",
		OwnerReferences:   ownerRefs,
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
	}

	status := v1.PodStatus{
		Message:  "this is message",
		Phase:    v1.PodRunning,
		QOSClass: v1.PodQOSBurstable,
	}

	spec := v1.PodSpec{
		NodeName:  "test",
		Hostname:  "hostname",
		DNSPolicy: v1.DNSClusterFirst,
	}

	o := v1.Pod{
		ObjectMeta: metadata,
		Status:     status,
		Spec:       spec,
	}

	ch := mdh.GetChannel()
	updateMsg := &controllers.K8sMessage{Object: &o, ObjectType: "pods"}
	ch <- updateMsg

	update := &metadatapb.ResourceUpdate{
		ResourceVersion: "1",
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				QOSClass:         2,
				Phase:            2,
				NodeName:         "test",
				Hostname:         "hostname",
			},
		},
	}

	mockMdStore.
		EXPECT().
		AddResourceVersion("1", update).
		Return(nil)
	mockMdStore.
		EXPECT().
		GetSubscriberResourceVersion("cloud").
		Return("0", nil)
	mockMdStore.
		EXPECT().
		UpdateSubscriberResourceVersion("cloud", "1")

	more := mdh.ProcessNextSubscriberUpdate()
	assert.Equal(t, true, more)
	assert.Equal(t, 1, len(updates))
	wrapperPb := &cvmsgspb.V2CMessage{}
	proto.Unmarshal(updates[0], wrapperPb)
	updatePb := &cvmsgspb.MetadataUpdate{}
	err := types.UnmarshalAny(wrapperPb.Msg, updatePb)
	assert.Nil(t, err)

	update.PrevResourceVersion = "0"
	assert.Equal(t, update, updatePb.Update)
}

func TestMetadataTopicListener_HandleMessage(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)
	mockMdStore.
		EXPECT().
		GetMetadataUpdatesForSubscriber("cloud", "", "5").
		Return([]*metadatapb.ResourceUpdate{
			&metadatapb.ResourceUpdate{ResourceVersion: "1"},
			&metadatapb.ResourceUpdate{ResourceVersion: "2"},
			&metadatapb.ResourceUpdate{ResourceVersion: "3"},
		}, nil)
	isLeader := true
	mdh, _ := controllers.NewMetadataHandler(mockMdStore, &isLeader)
	updates := make([][]byte, 0)
	// Create Metadata Service controller.
	mdl, err := controllers.NewMetadataTopicListener(mockMdStore, mdh, func(topic string, b []byte) error {
		assert.Equal(t, controllers.MetadataRequestPublishTopic, topic)
		updates = append(updates, b)
		return nil
	})

	req := cvmsgspb.MetadataRequest{
		From: "",
		To:   "5",
	}
	reqPb, err := req.Marshal()
	assert.Nil(t, err)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = mdl.HandleMessage(&msg)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(updates))
	wrapperPb := &cvmsgspb.V2CMessage{}
	proto.Unmarshal(updates[0], wrapperPb)
	updatePb := &cvmsgspb.MetadataResponse{}
	err = types.UnmarshalAny(wrapperPb.Msg, updatePb)

	assert.Equal(t, 3, len(updatePb.Updates))
	for i, u := range updatePb.Updates {
		assert.Equal(t, fmt.Sprintf("%d", i+1), u.ResourceVersion)
	}
}
