package controllers_test

import (
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	k8stypes "k8s.io/apimachinery/pkg/types"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

func createEndpointsObject() *v1.Endpoints {
	addrs := make([]v1.EndpointAddress, 2)
	nodeName := "this-is-a-node"
	addrs[0] = v1.EndpointAddress{
		IP:       "127.0.0.1",
		Hostname: "host",
		NodeName: &nodeName,
		TargetRef: &v1.ObjectReference{
			Kind:      "Pod",
			Namespace: "pl",
			UID:       "abcd",
			Name:      "pod-name",
		},
	}

	nodeName2 := "node-a"
	addrs[1] = v1.EndpointAddress{
		IP:       "127.0.0.2",
		Hostname: "host-2",
		NodeName: &nodeName2,
		TargetRef: &v1.ObjectReference{
			Kind:      "Pod",
			Namespace: "pl",
			UID:       "efgh",
			Name:      "another-pod",
		},
	}

	nodeName3 := "node-b"
	notReadyAddrs := []v1.EndpointAddress{
		v1.EndpointAddress{
			IP:       "127.0.0.3",
			Hostname: "host-3",
			NodeName: &nodeName3,
		},
	}

	ports := []v1.EndpointPort{
		v1.EndpointPort{
			Name:     "endpt",
			Port:     10,
			Protocol: v1.ProtocolTCP,
		},
		v1.EndpointPort{
			Name:     "abcd",
			Port:     500,
			Protocol: v1.ProtocolTCP,
		},
	}

	subsets := []v1.EndpointSubset{
		v1.EndpointSubset{
			Addresses:         addrs,
			NotReadyAddresses: notReadyAddrs,
			Ports:             ports,
		},
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	oRefs := []metav1.OwnerReference{
		metav1.OwnerReference{
			Kind: "pod",
			Name: "test",
			UID:  "abcd",
		},
	}

	md := metav1.ObjectMeta{
		Name:              "object_md",
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
		OwnerReferences:   oRefs,
	}

	return &v1.Endpoints{
		ObjectMeta: md,
		Subsets:    subsets,
	}
}

func createServiceObject() *v1.Service {
	// Create service object.
	ports := []v1.ServicePort{
		v1.ServicePort{
			Name:     "endpt",
			Port:     10,
			Protocol: v1.ProtocolTCP,
			NodePort: 20,
		},
		v1.ServicePort{
			Name:     "another_port",
			Port:     50,
			Protocol: v1.ProtocolTCP,
			NodePort: 60,
		},
	}

	externalIPs := []string{"127.0.0.2", "127.0.0.3"}

	spec := v1.ServiceSpec{
		ClusterIP:             "127.0.0.1",
		LoadBalancerIP:        "127.0.0.4",
		ExternalName:          "hello",
		ExternalTrafficPolicy: v1.ServiceExternalTrafficPolicyTypeLocal,
		Type:                  v1.ServiceTypeExternalName,
		Ports:                 ports,
		ExternalIPs:           externalIPs,
	}

	ownerRefs := []metav1.OwnerReference{
		metav1.OwnerReference{
			Kind: "pod",
			Name: "test",
			UID:  "abcd",
		},
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	metadata := metav1.ObjectMeta{
		Name:              "object_md",
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		ClusterName:       "a_cluster",
		OwnerReferences:   ownerRefs,
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
	}

	return &v1.Service{
		ObjectMeta: metadata,
		Spec:       spec,
	}
}

func createPodObject() *v1.Pod {
	ownerRefs := []metav1.OwnerReference{
		metav1.OwnerReference{
			Kind: "pod",
			Name: "test",
			UID:  "abcd",
		},
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

	conditions := make([]v1.PodCondition, 1)
	conditions[0] = v1.PodCondition{
		Type:   v1.PodReady,
		Status: v1.ConditionTrue,
	}

	waitingState := v1.ContainerStateWaiting{
		Message: "container state message",
		Reason:  "container state reason",
	}
	containers := []v1.ContainerStatus{
		v1.ContainerStatus{
			Name:        "container1",
			ContainerID: "docker://test",
			State: v1.ContainerState{
				Waiting: &waitingState,
			},
		},
	}

	status := v1.PodStatus{
		Message:           "this is message",
		Reason:            "this is reason",
		Phase:             v1.PodRunning,
		Conditions:        conditions,
		ContainerStatuses: containers,
		QOSClass:          v1.PodQOSBurstable,
		HostIP:            "127.0.0.5",
	}

	spec := v1.PodSpec{
		NodeName:  "test",
		Hostname:  "hostname",
		DNSPolicy: v1.DNSClusterFirst,
	}

	return &v1.Pod{
		ObjectMeta: metadata,
		Status:     status,
		Spec:       spec,
	}
}

func createNodeObject() *v1.Node {
	ownerRefs := []metav1.OwnerReference{
		metav1.OwnerReference{
			Kind: "pod",
			Name: "test",
			UID:  "abcd",
		},
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

	nodeSpec := v1.NodeSpec{
		PodCIDR: "pod_cidr",
	}

	nodeAddrs := []v1.NodeAddress{
		v1.NodeAddress{
			Type:    v1.NodeInternalIP,
			Address: "test_addr",
		},
	}

	nodeStatus := v1.NodeStatus{
		Addresses: nodeAddrs,
		Phase:     v1.NodeRunning,
	}

	return &v1.Node{
		ObjectMeta: metadata,
		Status:     nodeStatus,
		Spec:       nodeSpec,
	}
}

func createNamespaceObject() *v1.Namespace {
	ownerRefs := []metav1.OwnerReference{
		metav1.OwnerReference{
			Kind: "pod",
			Name: "test",
			UID:  "abcd",
		},
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

	return &v1.Namespace{
		ObjectMeta: metadata,
	}
}

func TestEndpointsUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct endpoints object.
	o := createEndpointsObject()

	p := controllers.EndpointsUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, metav1.Unix(0, 6), *o.ObjectMeta.DeletionTimestamp)

	o.ObjectMeta.DeletionTimestamp = nil
	p.SetDeleted(o)
	assert.NotNil(t, o.ObjectMeta.DeletionTimestamp)
}

func TestEndpointsUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct endpoints object.
	o := createEndpointsObject()

	state := &controllers.ProcessorState{
		LeaderMsgs: make(map[k8stypes.UID]*v1.Endpoints),
	}
	p := controllers.EndpointsUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)

	// Validating endpoints with no nodename should fail.
	o.Subsets[0].Addresses[0].NodeName = nil
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
	updates, rvs := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))
	assert.Equal(t, 1, len(rvs))

	assert.Equal(t, "1", rvs[0])
	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Endpoints{
			Endpoints: expectedPb,
		},
	}, updates[0])
}

func TestEndpointsUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct endpoints object.
	o := createEndpointsObject()
	o.Subsets[0].Addresses = append(o.Subsets[0].Addresses, v1.EndpointAddress{
		Hostname: "host",
		TargetRef: &v1.ObjectReference{
			Kind:      "Pod",
			Namespace: "pl",
			UID:       "xyz",
			Name:      "other-pod",
		},
	})

	state := &controllers.ProcessorState{
		PodToIP: map[string]string{
			"pl/another-pod": "127.0.0.2",
			"pl/pod-name":    "127.0.0.1",
			"pl/other-pod":   "127.0.0.1",
		},
	}
	p := controllers.EndpointsUpdateProcessor{}
	updates := p.GetUpdatesToSend(o, state)
	assert.Equal(t, 3, len(updates))

	assert.Contains(t, updates, &controllers.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			ResourceVersion: "1",
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
			ResourceVersion: "1",
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
			ResourceVersion: "1",
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
		Topics: []string{controllers.KelvinUpdateChannel},
	})
}

func TestServiceUpdateProcessor(t *testing.T) {
	// Construct service object.
	o := createServiceObject()

	p := controllers.ServiceUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, metav1.Unix(0, 6), *o.ObjectMeta.DeletionTimestamp)

	o.ObjectMeta.DeletionTimestamp = nil
	p.SetDeleted(o)
	assert.NotNil(t, o.ObjectMeta.DeletionTimestamp)
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
	o.Spec.ClusterIP = "10.64.3.1"

	state := &controllers.ProcessorState{}
	p := controllers.ServiceUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.3.1/32", state.ServiceCIDR.String())

	// Next service should expand the mask.
	o.Spec.ClusterIP = "10.64.3.7"
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.3.0/29", state.ServiceCIDR.String())

	// This one shouldn't expand the mask, because it's already within the same range.
	o.Spec.ClusterIP = "10.64.3.2"
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.3.0/29", state.ServiceCIDR.String())

	// Another range expansion.
	o.Spec.ClusterIP = "10.64.4.1"
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, "10.64.0.0/21", state.ServiceCIDR.String())

	// Test on Services that do not have ClusterIP.
	o.Spec.ClusterIP = ""
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
	updates, rvs := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))
	assert.Equal(t, 1, len(rvs))

	assert.Equal(t, "1", rvs[0])
	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Service{
			Service: expectedPb,
		},
	}, updates[0])
}

func TestServiceUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct endpoints object.
	o := createServiceObject()

	state := &controllers.ProcessorState{}
	p := controllers.ServiceUpdateProcessor{}
	updates := p.GetUpdatesToSend(o, state)
	assert.Equal(t, 0, len(updates))
}

func TestPodUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct pod object.
	o := createPodObject()

	p := controllers.PodUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, metav1.Unix(0, 6), *o.ObjectMeta.DeletionTimestamp)

	o.ObjectMeta.DeletionTimestamp = nil
	p.SetDeleted(o)
	assert.NotNil(t, o.ObjectMeta.DeletionTimestamp)
}

func TestPodUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct pod object.
	o := createPodObject()
	o.Status.PodIP = "127.0.0.1"

	state := &controllers.ProcessorState{PodToIP: make(map[string]string)}
	p := controllers.PodUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)

	assert.Equal(t, []string{"127.0.0.1/32"}, state.PodCIDRs)
	assert.Equal(t, 0, len(state.PodToIP))

	o.ObjectMeta.DeletionTimestamp = nil
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
	updates, rvs := p.GetStoredProtos(o)
	assert.Equal(t, 2, len(updates))
	assert.Equal(t, 2, len(rvs))

	assert.Equal(t, "1_0", rvs[0])
	assert.Equal(t, &storepb.K8SResource{
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
	}, updates[0])
	assert.Equal(t, "1_1", rvs[1])
	assert.Equal(t, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: expectedPb,
		},
	}, updates[1])
}

func TestPodUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct endpoints object.
	o := createPodObject()

	state := &controllers.ProcessorState{}
	p := controllers.PodUpdateProcessor{}
	updates := p.GetUpdatesToSend(o, state)
	assert.Equal(t, 2, len(updates))

	containerUpdate := &controllers.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			ResourceVersion: "1_0",
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: &metadatapb.ContainerUpdate{
					CID:            "test",
					Name:           "container1",
					PodID:          "ijkl",
					PodName:        "object_md",
					ContainerState: metadatapb.CONTAINER_STATE_WAITING,
					Message:        "container state message",
					Reason:         "container state reason",
				},
			},
		},
		Topics: []string{"127.0.0.5", controllers.KelvinUpdateChannel},
	}
	assert.Contains(t, updates, containerUpdate)

	podUpdate := &controllers.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			ResourceVersion: "1_1",
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
		Topics: []string{"127.0.0.5", controllers.KelvinUpdateChannel},
	}
	assert.Contains(t, updates, podUpdate)
}

func TestNodeUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct pod object.
	o := createNodeObject()

	p := controllers.NodeUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, metav1.Unix(0, 6), *o.ObjectMeta.DeletionTimestamp)

	o.ObjectMeta.DeletionTimestamp = nil
	p.SetDeleted(o)
	assert.NotNil(t, o.ObjectMeta.DeletionTimestamp)
}

func TestNodeUpdateProcessor_ValidateUpdate(t *testing.T) {
	// Construct node object.
	o := createNodeObject()

	state := &controllers.ProcessorState{NodeToIP: make(map[string]string)}
	p := controllers.NodeUpdateProcessor{}
	resp := p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, 0, len(state.NodeToIP))

	o.ObjectMeta.DeletionTimestamp = nil
	resp = p.ValidateUpdate(o, state)
	assert.True(t, resp)
	assert.Equal(t, 1, len(state.NodeToIP))
	assert.Equal(t, "test_addr", state.NodeToIP["object_md"])
}

func TestNodeUpdateProcessor_GetStoredProtos(t *testing.T) {
	// Construct node object.
	o := createNodeObject()

	p := controllers.NodeUpdateProcessor{}
	// Check that the generated store proto matches expected.
	updates, rvs := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))
	assert.Equal(t, 1, len(rvs))

	assert.Equal(t, "1", rvs[0])
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
							Address: "test_addr",
						},
					},
				},
			},
		},
	}, updates[0])
}

func TestNodeUpdateProcessor_GetUpdatesToSend(t *testing.T) {
	// Construct node object.
	o := createNodeObject()

	state := &controllers.ProcessorState{}
	p := controllers.NodeUpdateProcessor{}
	updates := p.GetUpdatesToSend(o, state)
	assert.Equal(t, 0, len(updates))
}

func TestNamespaceUpdateProcessor_SetDeleted(t *testing.T) {
	// Construct namespace object.
	o := createNamespaceObject()

	p := controllers.NamespaceUpdateProcessor{}
	p.SetDeleted(o)
	assert.Equal(t, metav1.Unix(0, 6), *o.ObjectMeta.DeletionTimestamp)

	o.ObjectMeta.DeletionTimestamp = nil
	p.SetDeleted(o)
	assert.NotNil(t, o.ObjectMeta.DeletionTimestamp)
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
	updates, rvs := p.GetStoredProtos(o)
	assert.Equal(t, 1, len(updates))
	assert.Equal(t, 1, len(rvs))

	assert.Equal(t, "1", rvs[0])
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
	o := createNamespaceObject()

	state := &controllers.ProcessorState{NodeToIP: map[string]string{
		"node-1": "127.0.0.1",
		"node-2": "127.0.0.2",
	}}
	p := controllers.NamespaceUpdateProcessor{}
	updates := p.GetUpdatesToSend(o, state)
	assert.Equal(t, 1, len(updates))

	nsUpdate := &controllers.OutgoingUpdate{
		Update: &metadatapb.ResourceUpdate{
			ResourceVersion: "1",
			Update: &metadatapb.ResourceUpdate_NamespaceUpdate{
				NamespaceUpdate: &metadatapb.NamespaceUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
				},
			},
		},
		Topics: []string{controllers.KelvinUpdateChannel, "127.0.0.1", "127.0.0.2"},
	}
	assert.Equal(t, nsUpdate.Update, updates[0].Update)
	assert.Contains(t, updates[0].Topics, controllers.KelvinUpdateChannel)
	assert.Contains(t, updates[0].Topics, "127.0.0.1")
	assert.Contains(t, updates[0].Topics, "127.0.0.2")
}
