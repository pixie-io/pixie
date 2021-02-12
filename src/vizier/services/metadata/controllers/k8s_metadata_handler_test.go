package controllers_test

import (
	"fmt"
	"sort"
	"sync"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

func createEndpointsObject() *storepb.K8SResource {
	pb := &metadatapb.Endpoints{}
	proto.UnmarshalText(testutils.EndpointsPb, pb)

	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_Endpoints{
			Endpoints: pb,
		},
	}
}

func createServiceObject() *storepb.K8SResource {
	pb := &metadatapb.Service{}
	proto.UnmarshalText(testutils.ServicePb, pb)

	return &storepb.K8SResource{
		Resource: &storepb.K8SResource_Service{
			Service: pb,
		},
	}
}

func createPodObject() *storepb.K8SResource {
	pb := &metadatapb.Pod{}
	proto.UnmarshalText(testutils.PodPbWithContainers, pb)

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
					ClusterName:     "a_cluster",
					OwnerReferences: []*metadatapb.OwnerReference{
						&metadatapb.OwnerReference{
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
						&metadatapb.NodeAddress{
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
					ClusterName:     "a_cluster",
					OwnerReferences: []*metadatapb.OwnerReference{
						&metadatapb.OwnerReference{
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
			} else {
				if val, ok := s.ResourceStoreByTopic["unscoped"][int64(k)]; ok {
					updates = append(updates, val)
				}
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

func TestK8sMetadataHandler_GetUpdatesForIP(t *testing.T) {
	mds := &InMemoryStore{
		ResourceStoreByTopic: make(map[string]ResourceStore),
		RVStore:              map[string]int64{},
	}

	// Populate resource store.
	mds.RVStore[controllers.KelvinUpdateTopic] = 6

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
	mds.AddResourceUpdate(2, &storepb.K8SResourceUpdate{
		Update: nsUpdate,
	})

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
	mds.AddResourceUpdateForTopic(4, controllers.KelvinUpdateTopic, &storepb.K8SResourceUpdate{
		Update: svcUpdateKelvin,
	})

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
	mds.AddResourceUpdateForTopic(4, "127.0.0.1", &storepb.K8SResourceUpdate{
		Update: svcUpdate1,
	})
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
	mds.AddResourceUpdateForTopic(4, "127.0.0.2", &storepb.K8SResourceUpdate{
		Update: svcUpdate2,
	})

	containerUpdate := &metadatapb.ContainerUpdate{
		CID:            "test",
		Name:           "container1",
		PodID:          "ijkl",
		PodName:        "object_md",
		ContainerState: metadatapb.CONTAINER_STATE_WAITING,
		Message:        "container state message",
		Reason:         "container state reason",
	}
	mds.AddResourceUpdateForTopic(5, controllers.KelvinUpdateTopic, &storepb.K8SResourceUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 5,
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: containerUpdate,
			},
		},
	})

	mds.AddResourceUpdateForTopic(5, "127.0.0.1", &storepb.K8SResourceUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 5,
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: containerUpdate,
			},
		},
	})

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
						&metadatapb.PodCondition{
							Type:   metadatapb.READY,
							Status: metadatapb.STATUS_TRUE,
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
	mds.AddResourceUpdateForTopic(6, controllers.KelvinUpdateTopic, pu)
	mds.AddResourceUpdateForTopic(6, "127.0.0.1", pu)

	updateCh := make(chan *controllers.K8sResourceMessage)
	mdh := controllers.NewK8sMetadataHandler(updateCh, mds, nil)
	defer mdh.Stop()
	updates, err := mdh.GetUpdatesForIP("", 0, 0)
	assert.Nil(t, err)
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
	assert.Nil(t, err)
	assert.Equal(t, 2, len(updates))
	assert.Equal(t, nsUpdate, updates[0])
	svcUpdate2.PrevUpdateVersion = 2
	assert.Equal(t, svcUpdate2, updates[1])

	updates, err = mdh.GetUpdatesForIP("127.0.0.1", 0, 3)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(updates))
	assert.Equal(t, nsUpdate, updates[0])
}

func TestK8sMetadataHandler_ProcessUpdates(t *testing.T) {
	updateCh := make(chan *controllers.K8sResourceMessage)

	mds := &InMemoryStore{
		ResourceStoreByTopic: make(map[string]ResourceStore),
		RVStore:              map[string]int64{},
		FullResourceStore:    make(map[int64]*storepb.K8SResource),
	}
	mds.RVStore[controllers.KelvinUpdateTopic] = 3

	natsPort, natsCleanup := testingutils.StartNATS(t)
	nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
	if err != nil {
		t.Fatal(err)
	}
	defer natsCleanup()

	mdh := controllers.NewK8sMetadataHandler(updateCh, mds, nc)
	defer mdh.Stop()

	expectedMsg := &messages.VizierMessage{
		Msg: &messages.VizierMessage_K8SMetadataMessage{
			K8SMetadataMessage: &messages.K8SMetadataMessage{
				Msg: &messages.K8SMetadataMessage_K8SMetadataUpdate{
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
	// We should expect a message to be sent out to the kelvin topic and 127.0.0.1
	var wg sync.WaitGroup
	wg.Add(1)
	nc.Subscribe(fmt.Sprintf("%s/%s", controllers.K8sMetadataUpdateChannel, controllers.KelvinUpdateTopic), func(msg *nats.Msg) {
		m := &messages.VizierMessage{}
		err := proto.Unmarshal(msg.Data, m)
		assert.Nil(t, err)
		assert.Equal(t, int64(3), m.GetK8SMetadataMessage().GetK8SMetadataUpdate().PrevUpdateVersion)
		m.GetK8SMetadataMessage().GetK8SMetadataUpdate().PrevUpdateVersion = 0
		assert.Equal(t, expectedMsg, m)
		wg.Done()
	})
	wg.Add(1)
	nc.Subscribe(fmt.Sprintf("%s/127.0.0.1", controllers.K8sMetadataUpdateChannel), func(msg *nats.Msg) {
		m := &messages.VizierMessage{}
		err := proto.Unmarshal(msg.Data, m)
		assert.Nil(t, err)
		assert.Equal(t, int64(5), m.GetK8SMetadataMessage().GetK8SMetadataUpdate().PrevUpdateVersion)
		m.GetK8SMetadataMessage().GetK8SMetadataUpdate().PrevUpdateVersion = 0
		assert.Equal(t, expectedMsg, m)
		wg.Done()
	})

	// Process a node update, to populate the NodeIPs.
	// This will increment the current resource version to 4, so the next update should have a resource version of 5.
	node := createNodeObject()
	node.GetNode().Metadata.DeletionTimestampNS = 0
	updateCh <- &controllers.K8sResourceMessage{
		Object:     node,
		ObjectType: "nodes",
	}

	o := createNamespaceObject()
	updateCh <- &controllers.K8sResourceMessage{
		Object:     o,
		ObjectType: "namespaces",
	}

	wg.Wait()

	assert.Equal(t, int64(5), mds.RVStore[controllers.KelvinUpdateTopic])

	// Full resource updates should be stored.
	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: &metadatapb.Namespace{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					ClusterName:     "a_cluster",
					OwnerReferences: []*metadatapb.OwnerReference{
						&metadatapb.OwnerReference{
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
					ClusterName:     "a_cluster",
					OwnerReferences: []*metadatapb.OwnerReference{
						&metadatapb.OwnerReference{
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
						&metadatapb.NodeAddress{
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
			PrevUpdateVersion: 5,
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

	p := controllers.EndpointsUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetEndpoints().Metadata.DeletionTimestampNS)

	o.GetEndpoints().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetEndpoints().Metadata.DeletionTimestampNS)
}

func TestEndpointsUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct endpoints object.
	o := createEndpointsObject()

	state := &controllers.ProcessorState{
		LeaderMsgs: make(map[string]*metadatapb.Endpoints),
	}
	p := controllers.EndpointsUpdateProcessor{}
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

	p := controllers.EndpointsUpdateProcessor{}

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

	storedProtos := []*controllers.StoredUpdate{
		&controllers.StoredUpdate{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Endpoints{
					Endpoints: expectedPb,
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &controllers.ProcessorState{
		PodToIP: map[string]string{
			"pl/another-pod": "127.0.0.2",
			"pl/pod-name":    "127.0.0.1",
			"pl/other-pod":   "127.0.0.1",
		},
	}
	p := controllers.EndpointsUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 3, len(updates))

	assert.Contains(t, updates, &controllers.OutgoingUpdate{
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

	assert.Contains(t, updates, &controllers.OutgoingUpdate{
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

	assert.Contains(t, updates, &controllers.OutgoingUpdate{
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
		Topics: []string{controllers.KelvinUpdateTopic},
	})
}

func TestServiceUpdateProcessor(t *testing.T) {
	// Construct service object.
	o := createServiceObject()

	p := controllers.ServiceUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetService().Metadata.DeletionTimestampNS)

	o.GetService().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetService().Metadata.DeletionTimestampNS)
}

func TestServiceUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct service object.
	o := createServiceObject()

	state := &controllers.ProcessorState{}
	p := controllers.ServiceUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
}

func TestServiceUpdateProcessor_ServiceCIDRs(t *testing.T) {
	// Construct service object.
	o := createServiceObject()
	o.GetService().Spec.ClusterIP = "10.64.3.1"

	state := &controllers.ProcessorState{}
	p := controllers.ServiceUpdateProcessor{}
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

	p := controllers.ServiceUpdateProcessor{}

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

	storedProtos := []*controllers.StoredUpdate{
		&controllers.StoredUpdate{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Service{
					Service: expectedPb,
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &controllers.ProcessorState{}
	p := controllers.ServiceUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 0, len(updates))
}

func TestPodUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct pod object.
	o := createPodObject()

	p := controllers.PodUpdateProcessor{}
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
	o := createPodObject()
	o.GetPod().Status.PodIP = "127.0.0.1"

	state := &controllers.ProcessorState{PodToIP: make(map[string]string)}
	p := controllers.PodUpdateProcessor{}
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
	o := createPodObject()

	p := controllers.PodUpdateProcessor{}

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
	storedProtos := []*controllers.StoredUpdate{
		&controllers.StoredUpdate{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Container{
					Container: containerUpdate,
				},
			},
			UpdateVersion: 2,
		},
		&controllers.StoredUpdate{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Pod{
					Pod: podUpdate,
				},
			},
			UpdateVersion: 3,
		},
	}

	state := &controllers.ProcessorState{PodToIP: map[string]string{
		"/object_md": "127.0.0.5",
	}}
	p := controllers.PodUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 2, len(updates))

	cu := &controllers.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: containerUpdate,
			},
		},
		Topics: []string{controllers.KelvinUpdateTopic, "127.0.0.5"},
	}
	assert.Contains(t, updates, cu)

	pu := &controllers.OutgoingUpdate{
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
						&metadatapb.PodCondition{
							Type:   metadatapb.READY,
							Status: metadatapb.STATUS_TRUE,
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
		Topics: []string{"127.0.0.5", controllers.KelvinUpdateTopic},
	}
	assert.Contains(t, updates, pu)
}

func TestNodeUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct pod object.
	o := createNodeObject()

	p := controllers.NodeUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetNode().Metadata.DeletionTimestampNS)

	o.GetNode().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetNode().Metadata.DeletionTimestampNS)
}

func TestNodeUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct node object.
	o := createNodeObject()

	state := &controllers.ProcessorState{NodeToIP: make(map[string]string)}
	p := controllers.NodeUpdateProcessor{}
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

	p := controllers.NodeUpdateProcessor{}
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
					ClusterName:     "a_cluster",
					OwnerReferences: []*metadatapb.OwnerReference{
						&metadatapb.OwnerReference{
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
						&metadatapb.NodeAddress{
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
	storedProtos := []*controllers.StoredUpdate{
		&controllers.StoredUpdate{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Node{
					Node: &metadatapb.Node{
						Metadata: &metadatapb.ObjectMetadata{
							Name:            "object_md",
							UID:             "ijkl",
							ResourceVersion: "1",
							ClusterName:     "a_cluster",
							OwnerReferences: []*metadatapb.OwnerReference{
								&metadatapb.OwnerReference{
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
								&metadatapb.NodeAddress{
									Type:    metadatapb.NODE_ADDR_TYPE_INTERNAL_IP,
									Address: "127.0.0.1",
								},
							},
						},
					},
				},
			},
			UpdateVersion: 2,
		},
	}

	state := &controllers.ProcessorState{}
	p := controllers.NodeUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 0, len(updates))
}

func TestNamespaceUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct namespace object.
	o := createNamespaceObject()

	p := controllers.NamespaceUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, int64(6), o.GetNamespace().Metadata.DeletionTimestampNS)

	o.GetNamespace().Metadata.DeletionTimestampNS = 0
	p.SetDeleted(o)
	assert.NotEqual(t, 0, o.GetNamespace().Metadata.DeletionTimestampNS)
}

func TestNamespaceUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct namespace object.
	o := createNamespaceObject()

	state := &controllers.ProcessorState{}
	p := controllers.NamespaceUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
}

func TestNamespaceUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct namespace object.
	o := createNamespaceObject()

	p := controllers.NamespaceUpdateProcessor{}

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
					ClusterName:     "a_cluster",
					OwnerReferences: []*metadatapb.OwnerReference{
						&metadatapb.OwnerReference{
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

	storedProtos := []*controllers.StoredUpdate{
		&controllers.StoredUpdate{
			Update: &storepb.K8SResource{
				Resource: &storepb.K8SResource_Namespace{
					Namespace: &metadatapb.Namespace{
						Metadata: &metadatapb.ObjectMetadata{
							Name:            "object_md",
							UID:             "ijkl",
							ResourceVersion: "1",
							ClusterName:     "a_cluster",
							OwnerReferences: []*metadatapb.OwnerReference{
								&metadatapb.OwnerReference{
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

	state := &controllers.ProcessorState{NodeToIP: map[string]string{
		"node-1": "127.0.0.1",
		"node-2": "127.0.0.2",
	}}
	p := controllers.NamespaceUpdateProcessor{}
	updates := p.GetUpdatesToSend(storedProtos, state)
	assert.Equal(t, 1, len(updates))

	nsUpdate := &controllers.OutgoingUpdate{
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
		Topics: []string{controllers.KelvinUpdateTopic, "127.0.0.1", "127.0.0.2"},
	}
	assert.Equal(t, nsUpdate.Update, updates[0].Update)
	assert.Contains(t, updates[0].Topics, controllers.KelvinUpdateTopic)
	assert.Contains(t, updates[0].Topics, "127.0.0.1")
	assert.Contains(t, updates[0].Topics, "127.0.0.2")
}
