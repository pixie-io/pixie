/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package k8smeta_test

import (
	"fmt"
	"sort"
	"sync"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/controllers/k8smeta"
	"px.dev/pixie/src/vizier/services/metadata/controllers/testutils"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
)

func createEndpointsObject() *storepb.K8SResource {
	pb := &metadatapb.Endpoints{}
	err := proto.UnmarshalText(testutils.EndpointsPb, pb)
	if err != nil {
		return &storepb.K8SResource{}
	}

	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_Endpoints{
			Endpoints: pb,
		},
	}
}

func createServiceObject() *storepb.K8SResource {
	pb := &metadatapb.Service{}
	err := proto.UnmarshalText(testutils.ServicePb, pb)
	if err != nil {
		return &storepb.K8SResource{}
	}

	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_Service{
			Service: pb,
		},
	}
}

func createPodObject(state metadatapb.PodPhase) *storepb.K8SResource {
	pb := &metadatapb.Pod{}
	var pbText string

	switch state {
	case metadatapb.TERMINATED:
		pbText = testutils.TerminatedPodPb
	case metadatapb.PENDING:
		pbText = testutils.PendingPodPb
	default:
		pbText = testutils.PodPbWithContainers
	}

	err := proto.UnmarshalText(pbText, pb)
	if err != nil {
		return &storepb.K8SResource{}
	}

	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: pb,
		},
	}
}

func createNodeObject() *storepb.K8SResource {
	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_Node{
			Node: &metadatapb.Node{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
					DeletionTimestampNS: 6,
				},
				Spec: &metadatapb.NodeSpec{
					PodCIDR: "pod_cidr",
				},
				Status: &metadatapb.NodeStatus{
					Phase: metadatapb.NODE_PHASE_RUNNING,
					Addresses: []*metadatapb.NodeAddress{
						{
							Type:    metadatapb.NODE_ADDR_TYPE_INTERNAL_IP,
							Address: "127.0.0.1",
						},
					},
				},
			},
		},
	}
}

func createNamespaceObject() *storepb.K8SResource {
	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: &metadatapb.Namespace{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
					DeletionTimestampNS: 6,
				},
			},
		},
	}
}

func createReplicaSetObject() *storepb.K8SResource {
	pb := &metadatapb.ReplicaSet{}
	err := proto.UnmarshalText(testutils.ReplicaSetPb, pb)
	if err != nil {
		return &storepb.K8SResource{}
	}

	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_ReplicaSet{
			ReplicaSet: pb,
		},
	}
}

func createDeploymentObject() *storepb.K8SResource {
	pb := &metadatapb.Deployment{}
	err := proto.UnmarshalText(testutils.DeploymentPb, pb)
	if err != nil {
		return &storepb.K8SResource{}
	}

	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_Deployment{
			Deployment: pb,
		},
	}
}

type ResourceStore map[int64]*storepb.K8SResourceUpdate
type InMemoryStore struct {
	ResourceStoreByTopic map[string]ResourceStore
	RVStore              map[string]int64
	FullResourceStore    map[int64]*storepb.K8SResource
}

func (s *InMemoryStore) AddResourceUpdateForTopic(uv int64, topic string, r *storepb.K8SResourceUpdate) error {
	if _, ok := s.ResourceStoreByTopic[topic]; !ok {
		s.ResourceStoreByTopic[topic] = make(map[int64]*storepb.K8SResourceUpdate)
	}
	s.ResourceStoreByTopic[topic][uv] = r
	return nil
}

func (s *InMemoryStore) AddResourceUpdate(uv int64, r *storepb.K8SResourceUpdate) error {
	if _, ok := s.ResourceStoreByTopic["unscoped"]; !ok {
		s.ResourceStoreByTopic["unscoped"] = make(map[int64]*storepb.K8SResourceUpdate)
	}
	s.ResourceStoreByTopic["unscoped"][uv] = r
	return nil
}

func (s *InMemoryStore) AddFullResourceUpdate(uv int64, r *storepb.K8SResource) error {
	s.FullResourceStore[uv] = r
	return nil
}

func (s *InMemoryStore) FetchFullResourceUpdates(from int64, to int64) ([]*storepb.K8SResource, error) {
	return nil, nil
}

func (s *InMemoryStore) FetchResourceUpdates(topic string, from int64, to int64) ([]*storepb.K8SResourceUpdate, error) {
	updates := make([]*storepb.K8SResourceUpdate, 0)

	keys := make([]int, len(s.ResourceStoreByTopic[topic])+len(s.ResourceStoreByTopic["unscoped"]))
	keyIdx := 0
	for k := range s.ResourceStoreByTopic[topic] {
		keys[keyIdx] = int(k)
		keyIdx++
	}

	for k := range s.ResourceStoreByTopic["unscoped"] {
		keys[keyIdx] = int(k)
		keyIdx++
	}
	sort.Ints(keys)

	for _, k := range keys {
		if k >= int(from) && k < int(to) {
			if val, ok := s.ResourceStoreByTopic[topic][int64(k)]; ok {
				updates = append(updates, val)
			} else if val, ok := s.ResourceStoreByTopic["unscoped"][int64(k)]; ok {
				updates = append(updates, val)
			}
		}
	}

	return updates, nil
}

func (s *InMemoryStore) GetUpdateVersion(topic string) (int64, error) {
	return s.RVStore[topic], nil
}

func (s *InMemoryStore) SetUpdateVersion(topic string, uv int64) error {
	s.RVStore[topic] = uv
	return nil
}

func TestHandler_GetUpdatesForIP(t *testing.T) {
	mds := &InMemoryStore{
		ResourceStoreByTopic: make(map[string]ResourceStore),
		RVStore:              map[string]int64{},
	}

	lps := &testutils.InMemoryPodLabelStore{
		Store: make(map[string]string),
	}

	// Populate resource store.
	mds.RVStore[k8smeta.KelvinUpdateTopic] = 6

	nsUpdate := &metadatapb.ResourceUpdate{
		UpdateVersion: 2,
		Update: &metadatapb.ResourceUpdate_NamespaceUpdate{
			NamespaceUpdate: &metadatapb.NamespaceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
			},
		},
	}
	err := mds.AddResourceUpdate(2, &storepb.K8SResourceUpdate{
		Update: nsUpdate,
	})
	require.NoError(t, err)

	svcUpdateKelvin := &metadatapb.ResourceUpdate{
		UpdateVersion: 4,
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "a_namespace",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				PodIDs:           []string{"abcd", "xyz"},
				PodNames:         []string{"pod-name", "other-pod"},
			},
		},
	}
	err = mds.AddResourceUpdateForTopic(4, k8smeta.KelvinUpdateTopic, &storepb.K8SResourceUpdate{
		Update: svcUpdateKelvin,
	})
	require.NoError(t, err)

	svcUpdate1 := &metadatapb.ResourceUpdate{
		UpdateVersion: 4,
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "a_namespace",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				PodIDs:           []string{"abcd"},
				PodNames:         []string{"pod-name"},
			},
		},
	}
	err = mds.AddResourceUpdateForTopic(4, "127.0.0.1", &storepb.K8SResourceUpdate{
		Update: svcUpdate1,
	})
	require.NoError(t, err)

	svcUpdate2 := &metadatapb.ResourceUpdate{
		UpdateVersion: 4,
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "a_namespace",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				PodIDs:           []string{"xyz"},
				PodNames:         []string{"other-pod"},
			},
		},
	}
	err = mds.AddResourceUpdateForTopic(4, "127.0.0.2", &storepb.K8SResourceUpdate{
		Update: svcUpdate2,
	})
	require.NoError(t, err)

	containerUpdate := &metadatapb.ContainerUpdate{
		CID:            "test",
		Name:           "container1",
		PodID:          "ijkl",
		PodName:        "object_md",
		ContainerState: metadatapb.CONTAINER_STATE_WAITING,
		Message:        "container state message",
		Reason:         "container state reason",
	}
	err = mds.AddResourceUpdateForTopic(5, k8smeta.KelvinUpdateTopic, &storepb.K8SResourceUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 5,
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: containerUpdate,
			},
		},
	})
	require.NoError(t, err)

	err = mds.AddResourceUpdateForTopic(5, "127.0.0.1", &storepb.K8SResourceUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 5,
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: containerUpdate,
			},
		},
	})
	require.NoError(t, err)

	pu := &storepb.K8SResourceUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 3,
			Update: &metadatapb.ResourceUpdate_PodUpdate{
				PodUpdate: &metadatapb.PodUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					Namespace:        "",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
					QOSClass:         metadatapb.QOS_CLASS_BURSTABLE,
					ContainerIDs:     []string{"test"},
					ContainerNames:   []string{"container1"},
					Phase:            metadatapb.RUNNING,
					Conditions: []*metadatapb.PodCondition{
						{
							Type:   metadatapb.READY,
							Status: metadatapb.CONDITION_STATUS_TRUE,
						},
					},
					NodeName: "test",
					Hostname: "hostname",
					PodIP:    "",
					HostIP:   "127.0.0.5",
					Message:  "this is message",
					Reason:   "this is reason",
				},
			},
		},
	}
	err = mds.AddResourceUpdateForTopic(6, k8smeta.KelvinUpdateTopic, pu)
	require.NoError(t, err)
	err = mds.AddResourceUpdateForTopic(6, "127.0.0.1", pu)
	require.NoError(t, err)

	updateCh := make(chan *k8smeta.K8sResourceMessage)
	mdh := k8smeta.NewHandler(updateCh, mds, lps, nil)
	defer mdh.Stop()
	updates, err := mdh.GetUpdatesForIP("", 0, 0)
	require.NoError(t, err)
	assert.Equal(t, 4, len(updates))
	assert.Equal(t, nsUpdate, updates[0])
	svcUpdateKelvin.PrevUpdateVersion = 2
	assert.Equal(t, svcUpdateKelvin, updates[1])
	assert.Equal(t, &metadatapb.ResourceUpdate{
		UpdateVersion:     5,
		PrevUpdateVersion: 4,
		Update: &metadatapb.ResourceUpdate_ContainerUpdate{
			ContainerUpdate: containerUpdate,
		},
	}, updates[2])
	pu.Update.PrevUpdateVersion = 5
	assert.Equal(t, pu.Update, updates[3])

	updates, err = mdh.GetUpdatesForIP("127.0.0.2", 0, 0)
	require.NoError(t, err)
	assert.Equal(t, 2, len(updates))
	assert.Equal(t, nsUpdate, updates[0])
	svcUpdate2.PrevUpdateVersion = 2
	assert.Equal(t, svcUpdate2, updates[1])

	updates, err = mdh.GetUpdatesForIP("127.0.0.1", 0, 3)
	require.NoError(t, err)
	assert.Equal(t, 1, len(updates))
	assert.Equal(t, nsUpdate, updates[0])
}

func TestHandler_ProcessUpdates(t *testing.T) {
	updateCh := make(chan *k8smeta.K8sResourceMessage)

	mds := &InMemoryStore{
		ResourceStoreByTopic: make(map[string]ResourceStore),
		RVStore:              map[string]int64{},
		FullResourceStore:    make(map[int64]*storepb.K8SResource),
	}
	lps := &testutils.InMemoryPodLabelStore{
		Store: make(map[string]string),
	}

	mds.RVStore[k8smeta.KelvinUpdateTopic] = 3
	mds.RVStore["127.0.0.1"] = 3

	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	mdh := k8smeta.NewHandler(updateCh, mds, lps, nc)
	defer mdh.Stop()

	expectedNSMsg := &messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_K8SMetadataMessage{
			K8SMetadataMessage: &messagespb.K8SMetadataMessage{
				Msg: &messagespb.K8SMetadataMessage_K8SMetadataUpdate{
					K8SMetadataUpdate: &metadatapb.ResourceUpdate{
						Update: &metadatapb.ResourceUpdate_NamespaceUpdate{
							NamespaceUpdate: &metadatapb.NamespaceUpdate{
								UID:              "ijkl",
								Name:             "object_md",
								StartTimestampNS: 4,
								StopTimestampNS:  6,
							},
						},
						UpdateVersion: 5,
					},
				},
			},
		},
	}

	expectedNodeMsg := &messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_K8SMetadataMessage{
			K8SMetadataMessage: &messagespb.K8SMetadataMessage{
				Msg: &messagespb.K8SMetadataMessage_K8SMetadataUpdate{
					K8SMetadataUpdate: &metadatapb.ResourceUpdate{
						Update: &metadatapb.ResourceUpdate_NodeUpdate{
							NodeUpdate: &metadatapb.NodeUpdate{
								UID:              "ijkl",
								Name:             "object_md",
								StartTimestampNS: 4,
								Phase:            metadatapb.NODE_PHASE_RUNNING,
								PodCIDR:          "pod_cidr",
							},
						},
						UpdateVersion: 4,
					},
				},
			},
		},
	}
	// We should expect a message to be sent out to the kelvin topic and 127.0.0.1 for the node and namespace updates.
	var wg sync.WaitGroup
	wg.Add(2)
	_, err := nc.Subscribe(fmt.Sprintf("%s/%s", k8smeta.K8sMetadataUpdateChannel, k8smeta.KelvinUpdateTopic), func(msg *nats.Msg) {
		m := &messagespb.VizierMessage{}
		err := proto.Unmarshal(msg.Data, m)
		require.NoError(t, err)

		prevUpdateVersion := m.GetK8SMetadataMessage().GetK8SMetadataUpdate().PrevUpdateVersion
		// Reset prevUpdateVersion for proto comparison.
		m.GetK8SMetadataMessage().GetK8SMetadataUpdate().PrevUpdateVersion = 0

		if prevUpdateVersion == 3 {
			assert.Equal(t, expectedNodeMsg, m)
			wg.Done()
		} else if prevUpdateVersion == 4 {
			assert.Equal(t, expectedNSMsg, m)
			wg.Done()
		}
	})
	require.NoError(t, err)
	wg.Add(2)
	_, err = nc.Subscribe(fmt.Sprintf("%s/127.0.0.1", k8smeta.K8sMetadataUpdateChannel), func(msg *nats.Msg) {
		m := &messagespb.VizierMessage{}
		err := proto.Unmarshal(msg.Data, m)
		require.NoError(t, err)

		prevUpdateVersion := m.GetK8SMetadataMessage().GetK8SMetadataUpdate().PrevUpdateVersion
		// Reset prevUpdateVersion for proto comparison.
		m.GetK8SMetadataMessage().GetK8SMetadataUpdate().PrevUpdateVersion = 0

		if prevUpdateVersion == 3 {
			assert.Equal(t, expectedNodeMsg, m)
			wg.Done()
		} else if prevUpdateVersion == 4 {
			assert.Equal(t, expectedNSMsg, m)
			wg.Done()
		}
	})
	require.NoError(t, err)

	// Process a node update, to populate the NodeIPs.
	// This will increment the current resource version to 4, so the next update should have a resource version of 5.
	node := createNodeObject()
	node.GetNode().Metadata.DeletionTimestampNS = 0
	updateCh <- &k8smeta.K8sResourceMessage{
		Object:     node,
		ObjectType: "nodes",
	}

	o := createNamespaceObject()
	updateCh <- &k8smeta.K8sResourceMessage{
		Object:     o,
		ObjectType: "namespaces",
	}

	wg.Wait()

	assert.Equal(t, int64(5), mds.RVStore[k8smeta.KelvinUpdateTopic])

	// Full resource updates should be stored.
	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: &metadatapb.Namespace{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
					DeletionTimestampNS: 6,
				},
			},
		},
	}, mds.FullResourceStore[5])

	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Node{
			Node: &metadatapb.Node{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
				},
				Spec: &metadatapb.NodeSpec{
					PodCIDR: "pod_cidr",
				},
				Status: &metadatapb.NodeStatus{
					Phase: metadatapb.NODE_PHASE_RUNNING,
					Addresses: []*metadatapb.NodeAddress{
						{
							Type:    metadatapb.NODE_ADDR_TYPE_INTERNAL_IP,
							Address: "127.0.0.1",
						},
					},
				},
			},
		},
	}, mds.FullResourceStore[4])

	// Partial resource updates should be stored.
	assert.Equal(t, &storepb.K8SResourceUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion:     5,
			PrevUpdateVersion: 4,
			Update: &metadatapb.ResourceUpdate_NamespaceUpdate{
				NamespaceUpdate: &metadatapb.NamespaceUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
				},
			},
		},
	}, mds.ResourceStoreByTopic["unscoped"][5])
}

func TestEndpointsUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct endpoints object.
	o := createEndpointsObject()

	p := k8smeta.EndpointsUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetEndpoints().Metadata.DeletionTimestampNS)

	o.GetEndpoints().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetEndpoints().Metadata.DeletionTimestampNS)
}

func TestEndpointsUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct endpoints object.
	o := createEndpointsObject()

	state := &k8smeta.ProcessorState{
		LeaderMsgs: make(map[string]*metadatapb.Endpoints),
	}
	p := k8smeta.EndpointsUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)

	// Validating endpoints with no nodename should fail.
	o.GetEndpoints().Subsets[0].Addresses[0].NodeName = ""
	resp = p.ValidateUpdate(o, state)
	assert.False(t, resp)
}

func TestEndpointsUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct endpoints object.
	o := createEndpointsObject()

	p := k8smeta.EndpointsUpdateProcessor{}

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	// Check that the generated store proto matches expected.
	updates := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))

	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Endpoints{
			Endpoints: expectedPb,
		},
	}, updates[0])
}

func TestEndpointsUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct endpoints update.
	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	expectedPb.Subsets[0].Addresses = append(expectedPb.Subsets[0].Addresses, &metadatapb.EndpointAddress{
		Hostname: "host",
		TargetRef: &metadatapb.ObjectReference{
			Kind:      "Pod",
			Namespace: "pl",
			UID:       "xyz",
			Name:      "other-pod",
		},
	})

	storedProtos := []*k8smeta.StoredUpdate{
		{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Endpoints{
					Endpoints: expectedPb,
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &k8smeta.ProcessorState{
		PodToIP: map[string]string{
			"pl/another-pod": "127.0.0.2",
			"pl/pod-name":    "127.0.0.1",
			"pl/other-pod":   "127.0.0.1",
		},
	}
	p := k8smeta.EndpointsUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 3, len(updates))

	assert.Contains(t, updates, &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_ServiceUpdate{
				ServiceUpdate: &metadatapb.ServiceUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					Namespace:        "a_namespace",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
					PodIDs:           []string{"abcd", "xyz"},
					PodNames:         []string{"pod-name", "other-pod"},
				},
			},
		},
		Topics: []string{"127.0.0.1"},
	})

	assert.Contains(t, updates, &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_ServiceUpdate{
				ServiceUpdate: &metadatapb.ServiceUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					Namespace:        "a_namespace",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
					PodIDs:           []string{"efgh"},
					PodNames:         []string{"another-pod"},
				},
			},
		},
		Topics: []string{"127.0.0.2"},
	})

	assert.Contains(t, updates, &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_ServiceUpdate{
				ServiceUpdate: &metadatapb.ServiceUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					Namespace:        "a_namespace",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
					PodIDs:           []string{"abcd", "efgh", "xyz"},
					PodNames:         []string{"pod-name", "another-pod", "other-pod"},
				},
			},
		},
		Topics: []string{k8smeta.KelvinUpdateTopic},
	})
}

func TestServiceUpdateProcessor(t *testing.T) {
	// Construct service object.
	o := createServiceObject()

	p := k8smeta.ServiceUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetService().Metadata.DeletionTimestampNS)

	o.GetService().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetService().Metadata.DeletionTimestampNS)
}

func TestServiceUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct service object.
	o := createServiceObject()

	state := &k8smeta.ProcessorState{}
	p := k8smeta.ServiceUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
}

func TestServiceUpdateProcessor_ServiceCIDRs(t *testing.T) {
	// Construct service object.
	o := createServiceObject()
	o.GetService().Spec.ClusterIP = "10.64.3.1"

	state := &k8smeta.ProcessorState{}
	p := k8smeta.ServiceUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.3.1/32", state.ServiceCIDR.String())

	// Next service should expand the mask.
	o.GetService().Spec.ClusterIP = "10.64.3.7"
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.3.0/29", state.ServiceCIDR.String())

	// This one shouldn't expand the mask, because it's already within the same range.
	o.GetService().Spec.ClusterIP = "10.64.3.2"
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.3.0/29", state.ServiceCIDR.String())

	// Another range expansion.
	o.GetService().Spec.ClusterIP = "10.64.4.1"
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.0.0/21", state.ServiceCIDR.String())

	// Test on Services that do not have ClusterIP.
	o.GetService().Spec.ClusterIP = ""
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.0.0/21", state.ServiceCIDR.String())
}

func TestServiceUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct service object.
	o := createServiceObject()

	p := k8smeta.ServiceUpdateProcessor{}

	expectedPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	// Check that the generated store proto matches expected.
	updates := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))

	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Service{
			Service: expectedPb,
		},
	}, updates[0])
}

func TestServiceUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct endpoints object.
	expectedPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	storedProtos := []*k8smeta.StoredUpdate{
		{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Service{
					Service: expectedPb,
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &k8smeta.ProcessorState{}
	p := k8smeta.ServiceUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 1, len(updates))

	su := &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_ServiceUpdate{
				ServiceUpdate: &metadatapb.ServiceUpdate{
					UID:       "ijkl",
					Name:      "object_md",
					Namespace: "a_namespace",
					ExternalIPs: []string{
						"127.0.0.2",
						"127.0.0.3",
					},
					ClusterIP: "127.0.0.1",
				},
			},
		},
		Topics: []string{k8smeta.KelvinUpdateTopic},
	}
	assert.Equal(t, updates[0], su)
}

func TestPodUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct pod object.
	o := createPodObject(metadatapb.RUNNING)

	p := k8smeta.PodUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetPod().Metadata.DeletionTimestampNS)

	o.GetPod().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetPod().Metadata.DeletionTimestampNS)
	for _, c := range o.GetPod().Status.ContainerStatuses {
		assert.NotEqual(t, 0, c.StopTimestampNS)
	}
}

func TestPodUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct pod object.
	o := createPodObject(metadatapb.RUNNING)
	o.GetPod().Status.PodIP = "127.0.0.1"

	state := &k8smeta.ProcessorState{PodToIP: make(map[string]string)}
	p := k8smeta.PodUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)

	assert.Equal(t, []string{"127.0.0.1/32"}, state.PodCIDRs)
	assert.Equal(t, 0, len(state.PodToIP))

	o.GetPod().Metadata.DeletionTimestampNS = 0
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)

	assert.Equal(t, []string{"127.0.0.1/32"}, state.PodCIDRs)
	assert.Equal(t, 1, len(state.PodToIP))
	assert.Equal(t, "127.0.0.5", state.PodToIP["/object_md"])
}

func TestPodUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct pod object.
	o := createPodObject(metadatapb.RUNNING)

	p := k8smeta.PodUpdateProcessor{}

	expectedPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	// Check that the generated store proto matches expected.
	updates := p.GetStoredProtos(o)
	assert.Equal(t, 2, len(updates))

	assert.Contains(t, updates, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Container{
			Container: &metadatapb.ContainerUpdate{
				CID:            "test",
				Name:           "container1",
				PodID:          "ijkl",
				PodName:        "object_md",
				ContainerState: metadatapb.CONTAINER_STATE_WAITING,
				Message:        "container state message",
				Reason:         "container state reason",
				ContainerType:  metadatapb.CONTAINER_TYPE_DOCKER,
			},
		},
	})

	assert.Contains(t, updates, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: expectedPb,
		},
	})
}

func TestPodUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct endpoints object.
	podUpdate := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, podUpdate); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	containerUpdate := &metadatapb.ContainerUpdate{
		CID:            "test",
		Name:           "container1",
		PodID:          "ijkl",
		PodName:        "object_md",
		ContainerState: metadatapb.CONTAINER_STATE_WAITING,
		Message:        "container state message",
		Reason:         "container state reason",
	}
	storedProtos := []*k8smeta.StoredUpdate{
		{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Container{
					Container: containerUpdate,
				},
			},
			UpdateVersion: 2,
		},
		{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Pod{
					Pod: podUpdate,
				},
			},
			UpdateVersion: 3,
		},
	}

	state := &k8smeta.ProcessorState{}
	p := k8smeta.PodUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 2, len(updates))

	cu := &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: containerUpdate,
			},
		},
		Topics: []string{k8smeta.KelvinUpdateTopic, "127.0.0.5"},
	}
	assert.Contains(t, updates, cu)

	pu := &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 3,
			Update: &metadatapb.ResourceUpdate_PodUpdate{
				PodUpdate: &metadatapb.PodUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					Namespace:        "",
					Labels:           `{"app":"myApp1","project":"myProj1"}`,
					StartTimestampNS: 4,
					StopTimestampNS:  6,
					QOSClass:         metadatapb.QOS_CLASS_BURSTABLE,
					ContainerIDs:     []string{"test"},
					ContainerNames:   []string{"container1"},
					Phase:            metadatapb.RUNNING,
					Conditions: []*metadatapb.PodCondition{
						{
							Type:   metadatapb.READY,
							Status: metadatapb.CONDITION_STATUS_TRUE,
						},
					},
					NodeName: "test",
					Hostname: "hostname",
					PodIP:    "",
					HostIP:   "127.0.0.5",
					Message:  "this is message",
					Reason:   "this is reason",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
				},
			},
		},
		Topics: []string{k8smeta.KelvinUpdateTopic, "127.0.0.5"},
	}
	assert.Contains(t, updates, pu)
}

func TestPodUpdateProcessor_UpdatePodLabelStore(t *testing.T) {
	runningPod := createPodObject(metadatapb.RUNNING)
	terminatedPod := createPodObject(metadatapb.TERMINATED)

	pls := &testutils.InMemoryPodLabelStore{
		Store: make(map[string]string),
	}

	err := k8smeta.UpdatePodLabelStore(runningPod, pls)
	require.NoError(t, err)

	pods, err := pls.FetchPodsWithLabelKey("default", "env")
	require.NoError(t, err)
	assert.Empty(t, pods)
	pods, err = pls.FetchPodsWithLabelKey("default", "project")
	require.NoError(t, err)
	assert.Equal(t, []string{"object_md"}, pods)

	err = k8smeta.UpdatePodLabelStore(terminatedPod, pls)
	require.NoError(t, err)

	pods, err = pls.FetchPodsWithLabelKey("default", "project")
	require.NoError(t, err)
	assert.Empty(t, pods)
}

func TestNodeUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct pod object.
	o := createNodeObject()

	p := k8smeta.NodeUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetNode().Metadata.DeletionTimestampNS)

	o.GetNode().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetNode().Metadata.DeletionTimestampNS)
}

func TestNodeUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct node object.
	o := createNodeObject()

	state := &k8smeta.ProcessorState{NodeToIP: make(map[string]string)}
	p := k8smeta.NodeUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, 0, len(state.NodeToIP))

	o.GetNode().Metadata.DeletionTimestampNS = 0
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, 1, len(state.NodeToIP))
	assert.Equal(t, "127.0.0.1", state.NodeToIP["object_md"])
}

func TestNodeUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct node object.
	o := createNodeObject()

	p := k8smeta.NodeUpdateProcessor{}
	// Check that the generated store proto matches expected.
	updates := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))

	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Node{
			Node: &metadatapb.Node{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
					DeletionTimestampNS: 6,
				},
				Spec: &metadatapb.NodeSpec{
					PodCIDR: "pod_cidr",
				},
				Status: &metadatapb.NodeStatus{
					Phase: metadatapb.NODE_PHASE_RUNNING,
					Addresses: []*metadatapb.NodeAddress{
						{
							Type:    metadatapb.NODE_ADDR_TYPE_INTERNAL_IP,
							Address: "127.0.0.1",
						},
					},
				},
			},
		},
	}, updates[0])
}

func TestNodeUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct node object.
	storedProtos := []*k8smeta.StoredUpdate{
		{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Node{
					Node: &metadatapb.Node{
						Metadata: &metadatapb.ObjectMetadata{
							Name:            "object_md",
							UID:             "ijkl",
							ResourceVersion: "1",
							OwnerReferences: []*metadatapb.OwnerReference{
								{
									Kind: "pod",
									Name: "test",
									UID:  "abcd",
								},
							},
							CreationTimestampNS: 4,
							DeletionTimestampNS: 6,
						},
						Spec: &metadatapb.NodeSpec{
							PodCIDR: "pod_cidr",
						},
						Status: &metadatapb.NodeStatus{
							Addresses: []*metadatapb.NodeAddress{
								{
									Type:    metadatapb.NODE_ADDR_TYPE_INTERNAL_IP,
									Address: "127.0.0.1",
								},
							},
							Phase: metadatapb.NODE_PHASE_RUNNING,
							Conditions: []*metadatapb.NodeCondition{
								{
									Type:   metadatapb.NODE_CONDITION_MEMORY_PRESSURE,
									Status: metadatapb.CONDITION_STATUS_FALSE,
								},
								{
									Type:   metadatapb.NODE_CONDITION_READY,
									Status: metadatapb.CONDITION_STATUS_TRUE,
								},
							},
						},
					},
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &k8smeta.ProcessorState{NodeToIP: map[string]string{
		"object_md": "127.0.0.1",
		"node-2":    "127.0.0.2",
	}}
	p := k8smeta.NodeUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 1, len(updates))

	nodeUpdate := &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_NodeUpdate{
				NodeUpdate: &metadatapb.NodeUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
					Phase:            metadatapb.NODE_PHASE_RUNNING,
					PodCIDR:          "pod_cidr",
					Conditions: []*metadatapb.NodeCondition{
						{
							Type:   metadatapb.NODE_CONDITION_MEMORY_PRESSURE,
							Status: metadatapb.CONDITION_STATUS_FALSE,
						},
						{
							Type:   metadatapb.NODE_CONDITION_READY,
							Status: metadatapb.CONDITION_STATUS_TRUE,
						},
					},
				},
			},
		},
	}
	assert.Equal(t, nodeUpdate.Update, updates[0].Update)
	assert.Contains(t, updates[0].Topics, k8smeta.KelvinUpdateTopic)
	assert.Contains(t, updates[0].Topics, "127.0.0.1")
}

func TestNamespaceUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct namespace object.
	o := createNamespaceObject()

	p := k8smeta.NamespaceUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetNamespace().Metadata.DeletionTimestampNS)

	o.GetNamespace().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetNamespace().Metadata.DeletionTimestampNS)
}

func TestNamespaceUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct namespace object.
	o := createNamespaceObject()

	state := &k8smeta.ProcessorState{}
	p := k8smeta.NamespaceUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
}

func TestNamespaceUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct namespace object.
	o := createNamespaceObject()

	p := k8smeta.NamespaceUpdateProcessor{}

	// Check that the generated store proto matches expected.
	updates := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))

	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: &metadatapb.Namespace{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
					DeletionTimestampNS: 6,
				},
			},
		},
	}, updates[0])
}

func TestNamespaceUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct namespace object.
	expectedPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	storedProtos := []*k8smeta.StoredUpdate{
		{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Namespace{
					Namespace: &metadatapb.Namespace{
						Metadata: &metadatapb.ObjectMetadata{
							Name:            "object_md",
							UID:             "ijkl",
							ResourceVersion: "1",
							OwnerReferences: []*metadatapb.OwnerReference{
								{
									Kind: "pod",
									Name: "test",
									UID:  "abcd",
								},
							},
							CreationTimestampNS: 4,
							DeletionTimestampNS: 6,
						},
					},
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &k8smeta.ProcessorState{NodeToIP: map[string]string{
		"node-1": "127.0.0.1",
		"node-2": "127.0.0.2",
	}}
	p := k8smeta.NamespaceUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 1, len(updates))

	nsUpdate := &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_NamespaceUpdate{
				NamespaceUpdate: &metadatapb.NamespaceUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
				},
			},
		},
		Topics: []string{k8smeta.KelvinUpdateTopic, "127.0.0.1", "127.0.0.2"},
	}
	assert.Equal(t, nsUpdate.Update, updates[0].Update)
	assert.Contains(t, updates[0].Topics, k8smeta.KelvinUpdateTopic)
	assert.Contains(t, updates[0].Topics, "127.0.0.1")
	assert.Contains(t, updates[0].Topics, "127.0.0.2")
}

func TestReplicaSetUpdateProcessor(t *testing.T) {
	// Construct replicaset object.
	o := createReplicaSetObject()

	p := k8smeta.ReplicaSetUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetReplicaSet().Metadata.DeletionTimestampNS)

	o.GetReplicaSet().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetReplicaSet().Metadata.DeletionTimestampNS)
}

func TestReplicaSetUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct replicaset object.
	o := createReplicaSetObject()

	state := &k8smeta.ProcessorState{}
	p := k8smeta.ReplicaSetUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
}

func TestReplicaSetUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct replicaset object.
	o := createReplicaSetObject()

	p := k8smeta.ReplicaSetUpdateProcessor{}

	expectedPb := &metadatapb.ReplicaSet{}
	if err := proto.UnmarshalText(testutils.ReplicaSetPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	// Check that the generated store proto matches expected.
	updates := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))

	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_ReplicaSet{
			ReplicaSet: expectedPb,
		},
	}, updates[0])
}

func TestReplicaSetUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct replicaset object.
	expectedPb := &metadatapb.ReplicaSet{}
	if err := proto.UnmarshalText(testutils.ReplicaSetPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	storedProtos := []*k8smeta.StoredUpdate{
		{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_ReplicaSet{
					ReplicaSet: expectedPb,
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &k8smeta.ProcessorState{NodeToIP: map[string]string{
		"node-1": "127.0.0.1",
		"node-2": "127.0.0.2",
	}}

	p := k8smeta.ReplicaSetUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 1, len(updates))

	rsUpdate := &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_ReplicaSetUpdate{
				ReplicaSetUpdate: &metadatapb.ReplicaSetUpdate{
					UID:                  "12345",
					Name:                 "rs_1",
					StartTimestampNS:     4,
					StopTimestampNS:      6,
					Namespace:            "a_namespace",
					Replicas:             2,
					FullyLabeledReplicas: 2,
					ReadyReplicas:        1,
					AvailableReplicas:    1,
					ObservedGeneration:   10,
					RequestedReplicas:    3,
					Conditions: []*metadatapb.ReplicaSetCondition{
						{
							Type:   "1",
							Status: 2,
						}, {
							Type:   "2",
							Status: 1,
						},
					},
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							UID:  "1111",
							Name: "d1",
							Kind: "deployment",
						},
					},
				},
			},
		},
		Topics: []string{k8smeta.KelvinUpdateTopic, "127.0.0.1", "127.0.0.2"},
	}

	assert.Equal(t, rsUpdate.Update, updates[0].Update)
	assert.Contains(t, updates[0].Topics, k8smeta.KelvinUpdateTopic)
	assert.Contains(t, updates[0].Topics, "127.0.0.1")
	assert.Contains(t, updates[0].Topics, "127.0.0.2")
}

func TestDeploymentUpdateProcessor(t *testing.T) {
	// Construct deployment object.
	o := createDeploymentObject()
	p := k8smeta.DeploymentUpdateProcessor{}

	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetDeployment().Metadata.DeletionTimestampNS)

	o.GetDeployment().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetDeployment().Metadata.DeletionTimestampNS)
}

func TestDeploymentUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct deployment object.
	o := createDeploymentObject()
	p := k8smeta.DeploymentUpdateProcessor{}

	state := &k8smeta.ProcessorState{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
}

func TestDeploymentUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct deployment object.
	o := createDeploymentObject()
	p := k8smeta.DeploymentUpdateProcessor{}

	expectedPb := &metadatapb.Deployment{}
	if err := proto.UnmarshalText(testutils.DeploymentPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	// Check that the generated store proto matches expected.
	updates := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))

	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Deployment{
			Deployment: expectedPb,
		},
	}, updates[0])
}

func TestDeploymentUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct deployment object.
	expectedPb := &metadatapb.Deployment{}
	if err := proto.UnmarshalText(testutils.DeploymentPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	storedProtos := []*k8smeta.StoredUpdate{
		{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Deployment{
					Deployment: expectedPb,
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &k8smeta.ProcessorState{NodeToIP: map[string]string{
		"node-1": "127.0.0.1",
		"node-2": "127.0.0.2",
	}}

	p := k8smeta.DeploymentUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 1, len(updates))

	depUpdate := &k8smeta.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_DeploymentUpdate{
				DeploymentUpdate: &metadatapb.DeploymentUpdate{
					UID:                 "ijkl",
					Name:                "deployment_1",
					StartTimestampNS:    4,
					StopTimestampNS:     6,
					Namespace:           "a_namespace",
					ObservedGeneration:  2,
					Replicas:            4,
					UpdatedReplicas:     3,
					ReadyReplicas:       2,
					AvailableReplicas:   3,
					UnavailableReplicas: 1,
					RequestedReplicas:   3,
					Conditions: []*metadatapb.DeploymentCondition{
						{
							Type:                 metadatapb.DEPLOYMENT_CONDITION_AVAILABLE,
							Status:               metadatapb.CONDITION_STATUS_TRUE,
							LastUpdateTimeNS:     4,
							LastTransitionTimeNS: 5,
							Reason:               "DeploymentAvailable",
							Message:              "Deployment replicas are available",
						},
						{
							Type:                 metadatapb.DEPLOYMENT_CONDITION_PROGRESSING,
							Status:               metadatapb.CONDITION_STATUS_TRUE,
							LastUpdateTimeNS:     4,
							LastTransitionTimeNS: 5,
							Reason:               "ReplicaUpdate",
							Message:              "Updated Replica",
						},
					},
				},
			},
		},
		Topics: []string{k8smeta.KelvinUpdateTopic, "127.0.0.1", "127.0.0.2"},
	}

	assert.Equal(t, depUpdate.Update, updates[0].Update)
	assert.Contains(t, updates[0].Topics, k8smeta.KelvinUpdateTopic)
	assert.Contains(t, updates[0].Topics, "127.0.0.1")
	assert.Contains(t, updates[0].Topics, "127.0.0.2")
}
