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
	"pixielabs.ai/pixielabs/src/vizier/utils/messagebus"
)

func TestMetadataTopicListener_MetadataSubscriber(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)
	mockMdStore.
		EXPECT().
		UpdateSubscriberResourceVersion("cloud", "")
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
		Reason:   "this is reason",
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
		ResourceVersion: "1_0",
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
				Message:          "this is message",
				Reason:           "this is reason",
			},
		},
	}

	mockMdStore.
		EXPECT().
		GetSubscriberResourceVersion("cloud").
		Return("", nil)
	mockMdStore.
		EXPECT().
		GetMetadataUpdates(nil).
		Return([]*metadatapb.ResourceUpdate{
			&metadatapb.ResourceUpdate{
				ResourceVersion:     "0",
				PrevResourceVersion: "",
			},
		}, nil)
	mockMdStore.
		EXPECT().
		UpdateSubscriberResourceVersion("cloud", "0")
	mockMdStore.
		EXPECT().
		UpdateSubscriberResourceVersion("cloud", "1_0")

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

func TestMetadataTopicListener_ProcessMessage(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)
	mockMdStore.
		EXPECT().
		GetMetadataUpdates(nil).
		Return([]*metadatapb.ResourceUpdate{
			&metadatapb.ResourceUpdate{ResourceVersion: "6"},
			&metadatapb.ResourceUpdate{ResourceVersion: "1"},
			&metadatapb.ResourceUpdate{ResourceVersion: "3"},
			&metadatapb.ResourceUpdate{ResourceVersion: "5"},
			&metadatapb.ResourceUpdate{ResourceVersion: "2"},
			&metadatapb.ResourceUpdate{ResourceVersion: "4"},
		}, nil)
	mockMdStore.
		EXPECT().
		UpdateSubscriberResourceVersion("cloud", "")

	isLeader := true
	mdh, _ := controllers.NewMetadataHandler(mockMdStore, &isLeader)
	updates := make([][]byte, 0)
	// Create Metadata Service controller.
	mdl, err := controllers.NewMetadataTopicListener(mockMdStore, mdh, func(topic string, b []byte) error {
		assert.Equal(t, messagebus.V2CTopic("1234"), topic)
		updates = append(updates, b)
		return nil
	})

	req := cvmsgspb.MetadataRequest{
		From:  "",
		To:    "5",
		Topic: "1234",
	}
	reqAnyMsg, err := types.MarshalAny(&req)
	assert.Nil(t, err)
	wrappedReq := cvmsgspb.C2VMessage{
		Msg: reqAnyMsg,
	}
	b, err := wrappedReq.Marshal()
	assert.Nil(t, err)

	msg := nats.Msg{}
	msg.Data = b
	err = mdl.ProcessMessage(&msg)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(updates))
	wrapperPb := &cvmsgspb.V2CMessage{}
	proto.Unmarshal(updates[0], wrapperPb)
	updatePb := &cvmsgspb.MetadataResponse{}
	err = types.UnmarshalAny(wrapperPb.Msg, updatePb)

	assert.Equal(t, 6, len(updatePb.Updates))
	for i, u := range updatePb.Updates {
		assert.Equal(t, fmt.Sprintf("%d", i+1), u.ResourceVersion)
	}
}

func TestMetadataTopicListener_ProcessMessageBatch(t *testing.T) {
	tests := []struct {
		name               string
		numUpdates         int
		expectedNumBatches int
	}{
		{
			name:               "single update",
			numUpdates:         1,
			expectedNumBatches: 1,
		},
		{
			name:               "batch sized update",
			numUpdates:         24,
			expectedNumBatches: 1,
		},
		{
			name:               "greater than one batch update",
			numUpdates:         25,
			expectedNumBatches: 2,
		},
		{
			name:               "multiple batches",
			numUpdates:         80,
			expectedNumBatches: 4,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			// Set up mock.
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

			retUpdates := make([]*metadatapb.ResourceUpdate, test.numUpdates)
			for i := 0; i < test.numUpdates; i++ {
				retUpdates[i] = &metadatapb.ResourceUpdate{ResourceVersion: fmt.Sprintf("%d", i)}
			}

			mockMdStore.
				EXPECT().
				GetMetadataUpdatesForHostname(nil, "0", fmt.Sprintf("%d", test.numUpdates+1)).
				Return(retUpdates, nil)

			mockMdStore.
				EXPECT().
				UpdateSubscriberResourceVersion("cloud", "")

			isLeader := true
			mdh, _ := controllers.NewMetadataHandler(mockMdStore, &isLeader)
			updates := make([]*metadatapb.ResourceUpdate, 0)
			// Create Metadata Service controller.
			numBatches := 0
			mdl, err := controllers.NewMetadataTopicListener(mockMdStore, mdh, func(topic string, b []byte) error {
				assert.Equal(t, messagebus.V2CTopic("1234"), topic)
				// updates = append(updates, b)
				wrapperPb := &cvmsgspb.V2CMessage{}
				proto.Unmarshal(b, wrapperPb)
				updatePb := &cvmsgspb.MetadataResponse{}
				err := types.UnmarshalAny(wrapperPb.Msg, updatePb)
				assert.Nil(t, err)
				updates = append(updates, updatePb.Updates...)

				numBatches++
				return nil
			})

			req := cvmsgspb.MetadataRequest{
				From:  "0",
				To:    fmt.Sprintf("%d", test.numUpdates+1),
				Topic: "1234",
			}
			reqAnyMsg, err := types.MarshalAny(&req)
			assert.Nil(t, err)
			wrappedReq := cvmsgspb.C2VMessage{
				Msg: reqAnyMsg,
			}
			b, err := wrappedReq.Marshal()
			assert.Nil(t, err)

			msg := nats.Msg{}
			msg.Data = b
			err = mdl.ProcessMessage(&msg)
			assert.Nil(t, err)
			assert.Equal(t, numBatches, test.expectedNumBatches)

			assert.Equal(t, test.numUpdates, len(updates))
			for i, u := range updates {
				assert.Equal(t, fmt.Sprintf("%d", i), u.ResourceVersion)
			}
		})
	}
}
