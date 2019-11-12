package controllers_test

import (
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
)

func TestObjectToEndpointsProto(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	ag1UpdatePb := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "a_namespace",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				PodIDs:           []string{"abcd"},
			},
		},
	}
	ag1Update, err := ag1UpdatePb.Marshal()

	ag2UpdatePb := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "a_namespace",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				PodIDs:           []string{"efgh"},
			},
		},
	}
	ag2Update, err := ag2UpdatePb.Marshal()

	mockMds.
		EXPECT().
		UpdateEndpoints(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"this-is-a-node"}).
		Return(&[]string{"agent-1"}, nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"node-a"}).
		Return(&[]string{"agent-2"}, nil)

	var wg sync.WaitGroup
	wg.Add(2)
	defer wg.Wait()

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-1", string(ag1Update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return nil
		})

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-2", string(ag2Update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return nil
		})

	// Create endpoints object.
	or := v1.ObjectReference{
		Kind:      "Pod",
		Namespace: "pl",
		UID:       "abcd",
	}

	or2 := v1.ObjectReference{
		Kind:      "Pod",
		Namespace: "pl",
		UID:       "efgh",
	}

	addrs := make([]v1.EndpointAddress, 2)
	nodeName := "this-is-a-node"
	addrs[0] = v1.EndpointAddress{
		IP:        "127.0.0.1",
		Hostname:  "host",
		NodeName:  &nodeName,
		TargetRef: &or,
	}

	nodeName2 := "node-a"
	addrs[1] = v1.EndpointAddress{
		IP:        "127.0.0.2",
		Hostname:  "host-2",
		NodeName:  &nodeName2,
		TargetRef: &or2,
	}

	notReadyAddrs := make([]v1.EndpointAddress, 1)
	nodeName3 := "node-b"
	notReadyAddrs[0] = v1.EndpointAddress{
		IP:       "127.0.0.3",
		Hostname: "host-3",
		NodeName: &nodeName3,
	}

	ports := make([]v1.EndpointPort, 2)
	ports[0] = v1.EndpointPort{
		Name:     "endpt",
		Port:     10,
		Protocol: v1.ProtocolTCP,
	}
	ports[1] = v1.EndpointPort{
		Name:     "abcd",
		Port:     500,
		Protocol: v1.ProtocolTCP,
	}

	subsets := make([]v1.EndpointSubset, 1)
	subsets[0] = v1.EndpointSubset{
		Addresses:         addrs,
		NotReadyAddresses: notReadyAddrs,
		Ports:             ports,
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	oRef := metav1.OwnerReference{
		Kind: "pod",
		Name: "test",
		UID:  "abcd",
	}

	oRefs := make([]metav1.OwnerReference, 1)
	oRefs[0] = oRef
	md := metav1.ObjectMeta{
		Name:              "object_md",
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
		OwnerReferences:   oRefs,
	}

	o := v1.Endpoints{
		ObjectMeta: md,
		Subsets:    subsets,
	}

	isLeader := true
	mh, err := controllers.NewMetadataHandler(mockMds, &isLeader)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "endpoints"}
	ch <- msg

	more := mh.ProcessNextAgentUpdate()
	assert.Equal(t, true, more)
	more = mh.ProcessNextAgentUpdate()
	assert.Equal(t, true, more)
}

func TestNoHostnameResolvedProto(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mockMds.
		EXPECT().
		UpdateEndpoints(expectedPb).
		Return(nil)

	var wg sync.WaitGroup
	wg.Add(1)
	defer wg.Wait()

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"this-is-a-node"}).
		DoAndReturn(func(hostnames *[]string) (*[]string, error) {
			wg.Done()
			return nil, nil
		})

	// Create endpoints object.
	or := v1.ObjectReference{
		Kind:      "Pod",
		Namespace: "pl",
		UID:       "abcd",
	}

	or2 := v1.ObjectReference{
		Kind:      "Pod",
		Namespace: "pl",
		UID:       "efgh",
	}

	addrs := make([]v1.EndpointAddress, 2)
	nodeName := "this-is-a-node"
	addrs[0] = v1.EndpointAddress{
		IP:        "127.0.0.1",
		Hostname:  "host",
		NodeName:  &nodeName,
		TargetRef: &or,
	}

	nodeName2 := "node-a"
	addrs[1] = v1.EndpointAddress{
		IP:        "127.0.0.2",
		Hostname:  "host-2",
		NodeName:  &nodeName2,
		TargetRef: &or2,
	}

	notReadyAddrs := make([]v1.EndpointAddress, 1)
	nodeName3 := "node-b"
	notReadyAddrs[0] = v1.EndpointAddress{
		IP:       "127.0.0.3",
		Hostname: "host-3",
		NodeName: &nodeName3,
	}

	ports := make([]v1.EndpointPort, 2)
	ports[0] = v1.EndpointPort{
		Name:     "endpt",
		Port:     10,
		Protocol: v1.ProtocolTCP,
	}
	ports[1] = v1.EndpointPort{
		Name:     "abcd",
		Port:     500,
		Protocol: v1.ProtocolTCP,
	}

	subsets := make([]v1.EndpointSubset, 1)
	subsets[0] = v1.EndpointSubset{
		Addresses:         addrs,
		NotReadyAddresses: notReadyAddrs,
		Ports:             ports,
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	oRef := metav1.OwnerReference{
		Kind: "pod",
		Name: "test",
		UID:  "abcd",
	}

	oRefs := make([]metav1.OwnerReference, 1)
	oRefs[0] = oRef
	md := metav1.ObjectMeta{
		Name:              "object_md",
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
		OwnerReferences:   oRefs,
	}

	o := v1.Endpoints{
		ObjectMeta: md,
		Subsets:    subsets,
	}

	isLeader := true
	mh, err := controllers.NewMetadataHandler(mockMds, &isLeader)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "endpoints"}
	ch <- msg

	more := mh.ProcessNextAgentUpdate()
	assert.Equal(t, true, more)
}

func TestAddToAgentUpdateQueueFailed(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	refs := make([]*metadatapb.ObjectReference, 1)
	refs[0] = &metadatapb.ObjectReference{
		Kind:      "Pod",
		Namespace: "pl",
		UID:       "abcd",
	}

	ag1UpdatePb := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "a_namespace",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				PodIDs:           []string{"abcd"},
			},
		},
	}
	ag1Update, err := ag1UpdatePb.Marshal()

	ag2UpdatePb := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "a_namespace",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				PodIDs:           []string{"efgh"},
			},
		},
	}
	ag2Update, err := ag2UpdatePb.Marshal()

	mockMds.
		EXPECT().
		UpdateEndpoints(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"this-is-a-node"}).
		Return(&[]string{"agent-1"}, nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"node-a"}).
		Return(&[]string{"agent-2"}, nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"node-a"}).
		Return(&[]string{"agent-3"}, nil)

	var wg sync.WaitGroup
	wg.Add(3)
	defer wg.Wait()

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-1", string(ag1Update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return nil
		})

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-2", string(ag2Update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return errors.New("Could not add to agent queue")
		})

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-3", string(ag2Update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return nil
		})

	// Create endpoints object.
	or := v1.ObjectReference{
		Kind:      "Pod",
		Namespace: "pl",
		UID:       "abcd",
	}

	or2 := v1.ObjectReference{
		Kind:      "Pod",
		Namespace: "pl",
		UID:       "efgh",
	}

	addrs := make([]v1.EndpointAddress, 2)
	nodeName := "this-is-a-node"
	addrs[0] = v1.EndpointAddress{
		IP:        "127.0.0.1",
		Hostname:  "host",
		NodeName:  &nodeName,
		TargetRef: &or,
	}

	nodeName2 := "node-a"
	addrs[1] = v1.EndpointAddress{
		IP:        "127.0.0.2",
		Hostname:  "host-2",
		NodeName:  &nodeName2,
		TargetRef: &or2,
	}

	notReadyAddrs := make([]v1.EndpointAddress, 1)
	nodeName3 := "node-b"
	notReadyAddrs[0] = v1.EndpointAddress{
		IP:       "127.0.0.3",
		Hostname: "host-3",
		NodeName: &nodeName3,
	}

	ports := make([]v1.EndpointPort, 2)
	ports[0] = v1.EndpointPort{
		Name:     "endpt",
		Port:     10,
		Protocol: v1.ProtocolTCP,
	}
	ports[1] = v1.EndpointPort{
		Name:     "abcd",
		Port:     500,
		Protocol: v1.ProtocolTCP,
	}

	subsets := make([]v1.EndpointSubset, 1)
	subsets[0] = v1.EndpointSubset{
		Addresses:         addrs,
		NotReadyAddresses: notReadyAddrs,
		Ports:             ports,
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	oRef := metav1.OwnerReference{
		Kind: "pod",
		Name: "test",
		UID:  "abcd",
	}

	oRefs := make([]metav1.OwnerReference, 1)
	oRefs[0] = oRef
	md := metav1.ObjectMeta{
		Name:              "object_md",
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
		OwnerReferences:   oRefs,
	}

	o := v1.Endpoints{
		ObjectMeta: md,
		Subsets:    subsets,
	}

	isLeader := true
	mh, err := controllers.NewMetadataHandler(mockMds, &isLeader)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "endpoints"}
	ch <- msg

	more := mh.ProcessNextAgentUpdate()
	assert.Equal(t, true, more)

	more = mh.ProcessNextAgentUpdate()
	assert.Equal(t, true, more)

	more = mh.ProcessNextAgentUpdate()
	assert.Equal(t, true, more)
}

func TestKubernetesEndpointHandler(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mockMds.
		EXPECT().
		UpdateEndpoints(gomock.Any()).
		Times(0)

	// Create endpoints object.
	addrs := make([]v1.EndpointAddress, 1)
	addrs[0] = v1.EndpointAddress{
		IP: "192.168.65.3",
	}

	ports := make([]v1.EndpointPort, 1)
	ports[0] = v1.EndpointPort{
		Name:     "https",
		Port:     6443,
		Protocol: v1.ProtocolTCP,
	}

	subsets := make([]v1.EndpointSubset, 1)
	subsets[0] = v1.EndpointSubset{
		Addresses: addrs,
		Ports:     ports,
	}

	creationTime := metav1.Unix(0, 4)
	oRef := metav1.OwnerReference{
		Kind: "pod",
		Name: "test",
		UID:  "abcd",
	}

	oRefs := make([]metav1.OwnerReference, 1)
	oRefs[0] = oRef
	md := metav1.ObjectMeta{
		Name:              "kubernetes",
		Namespace:         "default",
		UID:               "ijkl",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
		OwnerReferences:   oRefs,
	}

	o := v1.Endpoints{
		ObjectMeta: md,
		Subsets:    subsets,
	}

	isLeader := true
	mh, err := controllers.NewMetadataHandler(mockMds, &isLeader)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "endpoints"}
	ch <- msg
}

func TestObjectToServiceProto(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	var wg sync.WaitGroup
	wg.Add(1)
	defer wg.Wait()

	mockMds.
		EXPECT().
		UpdateService(expectedPb).
		DoAndReturn(func(*metadatapb.Service) error {
			wg.Done()
			return nil
		})

	// Create service object.
	ports := make([]v1.ServicePort, 2)
	ports[0] = v1.ServicePort{
		Name:     "endpt",
		Port:     10,
		Protocol: v1.ProtocolTCP,
		NodePort: 20,
	}
	ports[1] = v1.ServicePort{
		Name:     "another_port",
		Port:     50,
		Protocol: v1.ProtocolTCP,
		NodePort: 60,
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
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		ClusterName:       "a_cluster",
		OwnerReferences:   ownerRefs,
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
	}

	o := v1.Service{
		ObjectMeta: metadata,
		Spec:       spec,
	}

	isLeader := true
	mh, err := controllers.NewMetadataHandler(mockMds, &isLeader)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "services"}
	ch <- msg
}

func TestObjectToPodProto(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	updatePb := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_ContainerUpdate{
			ContainerUpdate: &metadatapb.ContainerUpdate{
				CID:  "test",
				Name: "container1",
			},
		},
	}

	update, err := updatePb.Marshal()

	mockMds.
		EXPECT().
		UpdatePod(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		UpdateContainersFromPod(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"test"}).
		Return(&[]string{"agent-1"}, nil)

	var wg sync.WaitGroup
	wg.Add(1)
	defer wg.Wait()

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-1", string(update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return nil
		})

	// Create service object.
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

	conditions := make([]v1.PodCondition, 1)
	conditions[0] = v1.PodCondition{
		Type: v1.PodReady,
	}

	containers := make([]v1.ContainerStatus, 1)
	waitingState := v1.ContainerStateWaiting{}

	containers[0] = v1.ContainerStatus{
		Name:        "container1",
		ContainerID: "docker://test",
		State: v1.ContainerState{
			Waiting: &waitingState,
		},
	}

	status := v1.PodStatus{
		Message:           "this is message",
		Phase:             v1.PodRunning,
		Conditions:        conditions,
		ContainerStatuses: containers,
		QOSClass:          v1.PodQOSBurstable,
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

	isLeader := true
	mh, err := controllers.NewMetadataHandler(mockMds, &isLeader)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "pods"}
	ch <- msg

	mh.ProcessNextAgentUpdate()
}

func TestGetResourceUpdateFromPod(t *testing.T) {
	pod := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, pod); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	update := controllers.GetResourceUpdateFromPod(pod)
	podUpdate := update.GetPodUpdate()
	assert.NotNil(t, podUpdate)
	assert.Equal(t, "ijkl", podUpdate.UID)
	assert.Equal(t, "object_md", podUpdate.Name)
	assert.Equal(t, 1, len(podUpdate.ContainerIDs))
	assert.Equal(t, int64(4), podUpdate.StartTimestampNS)
	assert.Equal(t, int64(6), podUpdate.StopTimestampNS)
	assert.Equal(t, "test", podUpdate.ContainerIDs[0])
	assert.Equal(t, metadatapb.QOS_CLASS_BURSTABLE, podUpdate.QOSClass)
}

func TestGetResourceUpdateFromEndpoints(t *testing.T) {
	ep := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, ep); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	update := controllers.GetResourceUpdateFromEndpoints(ep)
	serviceUpdate := update.GetServiceUpdate()
	assert.NotNil(t, serviceUpdate)
	assert.Equal(t, "object_md", serviceUpdate.Name)
	assert.Equal(t, "ijkl", serviceUpdate.UID)
	assert.Equal(t, int64(4), serviceUpdate.StartTimestampNS)
	assert.Equal(t, int64(6), serviceUpdate.StopTimestampNS)
	assert.Equal(t, 2, len(serviceUpdate.PodIDs))
	assert.Equal(t, "abcd", serviceUpdate.PodIDs[0])
	assert.Equal(t, "efgh", serviceUpdate.PodIDs[1])
}

func TestGetContainerResourceUpdatesFromPod(t *testing.T) {
	pod := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, pod); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	updates := controllers.GetContainerResourceUpdatesFromPod(pod)
	assert.Equal(t, 1, len(updates))
	cUpdate := updates[0].GetContainerUpdate()
	assert.NotNil(t, cUpdate)
	assert.Equal(t, "container1", cUpdate.Name)
	assert.Equal(t, "test", cUpdate.CID)
}

func TestSyncPodData(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	pods := make([]v1.Pod, 1)

	creationTime := metav1.Unix(0, 4)
	p1Md := metav1.ObjectMeta{
		Name:              "object_md",
		UID:               "active_pod",
		ResourceVersion:   "1",
		ClusterName:       "a_cluster",
		CreationTimestamp: creationTime,
	}

	p1Containers := make([]v1.ContainerStatus, 1)
	waitingState := v1.ContainerStateWaiting{}

	p1Containers[0] = v1.ContainerStatus{
		Name:        "container1",
		ContainerID: "docker://active_container",
		State: v1.ContainerState{
			Waiting: &waitingState,
		},
	}

	p1Status := v1.PodStatus{
		Message:           "this is message",
		Phase:             v1.PodRunning,
		ContainerStatuses: p1Containers,
		QOSClass:          v1.PodQOSBurstable,
	}

	activePod := v1.Pod{
		ObjectMeta: p1Md,
		Status:     p1Status,
	}
	pods[0] = activePod

	podList := v1.PodList{
		Items: pods,
	}

	// Test case 1: A pod that is active, and known to be active by etcd.
	etcdPods := make([](*metadatapb.Pod), 4)
	activePodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, activePodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	activePodPb.Metadata.UID = "active_pod"
	activePodPb.Metadata.DeletionTimestampNS = 0
	etcdPods[0] = activePodPb

	// Test case 2: A pod that is dead, and known to be dead by etcd.
	deadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, deadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deadPodPb.Metadata.DeletionTimestampNS = 10
	deadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 6
	etcdPods[1] = deadPodPb

	// Test case 3: A pod that is dead, but which etcd does not know to be dead.
	undeadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, undeadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	undeadPodPb.Metadata.UID = "undead_pod"
	undeadPodPb.Metadata.DeletionTimestampNS = 0
	undeadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 0
	etcdPods[2] = undeadPodPb

	deletedUndeadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, deletedUndeadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deletedUndeadPodPb.Metadata.UID = "undead_pod"
	deletedUndeadPodPb.Metadata.DeletionTimestampNS = 10
	deletedUndeadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 10

	// Test case 4: A pod that is dead, and known to be dead by etcd,
	// but for which etcd thinks the inner container is alive.
	anotherUndeadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, anotherUndeadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	anotherUndeadPodPb.Metadata.UID = "another_undead_pod"
	anotherUndeadPodPb.Metadata.DeletionTimestampNS = 10
	anotherUndeadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 0
	anotherUndeadPodPb.Status.ContainerStatuses[0].Name = "another_undead_container_name"
	anotherUndeadPodPb.Status.ContainerStatuses[0].ContainerID = "another_undead_container"
	etcdPods[3] = anotherUndeadPodPb

	anotherDeletedUndeadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, anotherDeletedUndeadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	anotherDeletedUndeadPodPb.Metadata.UID = "another_undead_pod"
	anotherDeletedUndeadPodPb.Metadata.DeletionTimestampNS = 10
	anotherDeletedUndeadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 10
	anotherDeletedUndeadPodPb.Status.ContainerStatuses[0].Name = "another_undead_container_name"
	anotherDeletedUndeadPodPb.Status.ContainerStatuses[0].ContainerID = "another_undead_container"

	etcdContainers := make([](*metadatapb.ContainerInfo), 4)
	activeContainerPb := &metadatapb.ContainerInfo{
		Name:             "active_container_name",
		UID:              "active_container",
		StartTimestampNS: 4,
	}
	etcdContainers[0] = activeContainerPb
	undeadContainerPb := &metadatapb.ContainerInfo{
		Name:             "undead_container_name",
		UID:              "undead_container",
		StartTimestampNS: 4,
	}
	etcdContainers[1] = undeadContainerPb
	deletedContainerPb := &metadatapb.ContainerInfo{
		Name:             "deleted_container_name",
		UID:              "deleted_container",
		StartTimestampNS: 4,
		StopTimestampNS:  10,
	}
	etcdContainers[2] = deletedContainerPb

	deletedUndeadContainerPb := &metadatapb.ContainerInfo{
		Name:             "undead_container_name",
		UID:              "undead_container",
		StartTimestampNS: 4,
		StopTimestampNS:  10,
	}

	anotherDeletedContainerPb := &metadatapb.ContainerInfo{
		Name:             "another_deleted_container_name",
		UID:              "another_deleted_container",
		StartTimestampNS: 4,
	}
	etcdContainers[3] = anotherDeletedContainerPb

	anotherDeletedUndeadContainerPb := &metadatapb.ContainerInfo{
		Name:             "another_deleted_container_name",
		UID:              "another_deleted_container",
		StartTimestampNS: 4,
		StopTimestampNS:  10,
	}

	mockMds.
		EXPECT().
		GetPods().
		Return(etcdPods, nil)

	mockMds.
		EXPECT().
		UpdatePod(deletedUndeadPodPb).
		Return(nil)

	mockMds.
		EXPECT().
		UpdatePod(anotherDeletedUndeadPodPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetContainers().
		Return(etcdContainers, nil)

	mockMds.
		EXPECT().
		UpdateContainer(deletedUndeadContainerPb).
		Return(nil)

	mockMds.
		EXPECT().
		UpdateContainer(anotherDeletedUndeadContainerPb).
		Return(nil)

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	isLeader := true
	mh, err := controllers.NewMetadataHandlerWithClock(mockMds, &isLeader, clock)
	assert.Nil(t, err)

	mh.SyncPodData(&podList)
}

func TestSyncPodData_NotLeader(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	pods := make([]v1.Pod, 1)

	creationTime := metav1.Unix(0, 4)
	p1Md := metav1.ObjectMeta{
		Name:              "object_md",
		UID:               "active_pod",
		ResourceVersion:   "1",
		ClusterName:       "a_cluster",
		CreationTimestamp: creationTime,
	}

	p1Containers := make([]v1.ContainerStatus, 1)
	waitingState := v1.ContainerStateWaiting{}

	p1Containers[0] = v1.ContainerStatus{
		Name:        "container1",
		ContainerID: "docker://active_container",
		State: v1.ContainerState{
			Waiting: &waitingState,
		},
	}

	p1Status := v1.PodStatus{
		Message:           "this is message",
		Phase:             v1.PodRunning,
		ContainerStatuses: p1Containers,
		QOSClass:          v1.PodQOSBurstable,
	}

	activePod := v1.Pod{
		ObjectMeta: p1Md,
		Status:     p1Status,
	}
	pods[0] = activePod

	podList := v1.PodList{
		Items: pods,
	}

	// Test case 1: A pod that is active, and known to be active by etcd.
	etcdPods := make([](*metadatapb.Pod), 4)
	activePodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, activePodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	activePodPb.Metadata.UID = "active_pod"
	activePodPb.Metadata.DeletionTimestampNS = 0
	etcdPods[0] = activePodPb

	// Test case 2: A pod that is dead, and known to be dead by etcd.
	deadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, deadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deadPodPb.Metadata.DeletionTimestampNS = 10
	deadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 6
	etcdPods[1] = deadPodPb

	// Test case 3: A pod that is dead, but which etcd does not know to be dead.
	undeadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, undeadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	undeadPodPb.Metadata.UID = "undead_pod"
	undeadPodPb.Metadata.DeletionTimestampNS = 0
	undeadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 0
	etcdPods[2] = undeadPodPb

	deletedUndeadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, deletedUndeadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deletedUndeadPodPb.Metadata.UID = "undead_pod"
	deletedUndeadPodPb.Metadata.DeletionTimestampNS = 10
	deletedUndeadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 10

	// Test case 4: A pod that is dead, and known to be dead by etcd,
	// but for which etcd thinks the inner container is alive.
	anotherUndeadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, anotherUndeadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	anotherUndeadPodPb.Metadata.UID = "another_undead_pod"
	anotherUndeadPodPb.Metadata.DeletionTimestampNS = 10
	anotherUndeadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 0
	anotherUndeadPodPb.Status.ContainerStatuses[0].Name = "another_undead_container_name"
	anotherUndeadPodPb.Status.ContainerStatuses[0].ContainerID = "another_undead_container"
	etcdPods[3] = anotherUndeadPodPb

	anotherDeletedUndeadPodPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, anotherDeletedUndeadPodPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	anotherDeletedUndeadPodPb.Metadata.UID = "another_undead_pod"
	anotherDeletedUndeadPodPb.Metadata.DeletionTimestampNS = 10
	anotherDeletedUndeadPodPb.Status.ContainerStatuses[0].StopTimestampNS = 10
	anotherDeletedUndeadPodPb.Status.ContainerStatuses[0].Name = "another_undead_container_name"
	anotherDeletedUndeadPodPb.Status.ContainerStatuses[0].ContainerID = "another_undead_container"

	etcdContainers := make([](*metadatapb.ContainerInfo), 4)
	activeContainerPb := &metadatapb.ContainerInfo{
		Name:             "active_container_name",
		UID:              "active_container",
		StartTimestampNS: 4,
	}
	etcdContainers[0] = activeContainerPb
	undeadContainerPb := &metadatapb.ContainerInfo{
		Name:             "undead_container_name",
		UID:              "undead_container",
		StartTimestampNS: 4,
	}
	etcdContainers[1] = undeadContainerPb
	deletedContainerPb := &metadatapb.ContainerInfo{
		Name:             "deleted_container_name",
		UID:              "deleted_container",
		StartTimestampNS: 4,
		StopTimestampNS:  10,
	}
	etcdContainers[2] = deletedContainerPb

	anotherDeletedContainerPb := &metadatapb.ContainerInfo{
		Name:             "another_deleted_container_name",
		UID:              "another_deleted_container",
		StartTimestampNS: 4,
	}
	etcdContainers[3] = anotherDeletedContainerPb

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	isLeader := false
	mh, err := controllers.NewMetadataHandlerWithClock(mockMds, &isLeader, clock)
	assert.Nil(t, err)

	mh.SyncPodData(&podList)
}

func TestSyncEndpointsData(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	endpoints := make([]v1.Endpoints, 1)
	// Create endpoints object.
	creationTime := metav1.Unix(0, 4)
	md := metav1.ObjectMeta{
		Name:              "kubernetes",
		Namespace:         "default",
		UID:               "active_ep",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
	}

	activeEndpoint := v1.Endpoints{
		ObjectMeta: md,
	}
	endpoints[0] = activeEndpoint

	epList := v1.EndpointsList{
		Items: endpoints,
	}

	etcdEps := make([](*metadatapb.Endpoints), 3)
	activeEpPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, activeEpPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	activeEpPb.Metadata.UID = "active_ep"
	activeEpPb.Metadata.DeletionTimestampNS = 0
	etcdEps[0] = activeEpPb

	deadEpPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, deadEpPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deadEpPb.Metadata.UID = "dead_ep"
	deadEpPb.Metadata.DeletionTimestampNS = 5
	etcdEps[1] = deadEpPb

	undeadEpPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, undeadEpPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	undeadEpPb.Metadata.UID = "undead_ep"
	undeadEpPb.Metadata.DeletionTimestampNS = 0
	etcdEps[2] = undeadEpPb

	deletedUndeadEpPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, deletedUndeadEpPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deletedUndeadEpPb.Metadata.UID = "undead_ep"
	deletedUndeadEpPb.Metadata.DeletionTimestampNS = 10

	mockMds.
		EXPECT().
		GetEndpoints().
		Return(etcdEps, nil)

	mockMds.
		EXPECT().
		UpdateEndpoints(deletedUndeadEpPb).
		Return(nil)

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	isLeader := true
	mh, err := controllers.NewMetadataHandlerWithClock(mockMds, &isLeader, clock)
	assert.Nil(t, err)

	mh.SyncEndpointsData(&epList)
}

func TestSyncEndpointsData_NotLeader(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	endpoints := make([]v1.Endpoints, 1)
	// Create endpoints object.
	creationTime := metav1.Unix(0, 4)
	md := metav1.ObjectMeta{
		Name:              "kubernetes",
		Namespace:         "default",
		UID:               "active_ep",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
	}

	activeEndpoint := v1.Endpoints{
		ObjectMeta: md,
	}
	endpoints[0] = activeEndpoint

	epList := v1.EndpointsList{
		Items: endpoints,
	}

	etcdEps := make([](*metadatapb.Endpoints), 3)
	activeEpPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, activeEpPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	activeEpPb.Metadata.UID = "active_ep"
	activeEpPb.Metadata.DeletionTimestampNS = 0
	etcdEps[0] = activeEpPb

	deadEpPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, deadEpPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deadEpPb.Metadata.UID = "dead_ep"
	deadEpPb.Metadata.DeletionTimestampNS = 5
	etcdEps[1] = deadEpPb

	undeadEpPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, undeadEpPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	undeadEpPb.Metadata.UID = "undead_ep"
	undeadEpPb.Metadata.DeletionTimestampNS = 0
	etcdEps[2] = undeadEpPb

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	isLeader := false
	mh, err := controllers.NewMetadataHandlerWithClock(mockMds, &isLeader, clock)
	assert.Nil(t, err)

	mh.SyncEndpointsData(&epList)
}

func TestSyncServicesData(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	services := make([]v1.Service, 1)
	// Create endpoints object.
	creationTime := metav1.Unix(0, 4)
	md := metav1.ObjectMeta{
		Name:              "kubernetes",
		Namespace:         "default",
		UID:               "active_service",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
	}

	activeService := v1.Service{
		ObjectMeta: md,
	}
	services[0] = activeService

	sList := v1.ServiceList{
		Items: services,
	}

	etcdServices := make([](*metadatapb.Service), 3)
	activeServicePb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, activeServicePb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	activeServicePb.Metadata.UID = "active_service"
	activeServicePb.Metadata.DeletionTimestampNS = 0
	etcdServices[0] = activeServicePb

	deadServicePb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, deadServicePb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deadServicePb.Metadata.UID = "dead_service"
	deadServicePb.Metadata.DeletionTimestampNS = 5
	etcdServices[1] = deadServicePb

	undeadServicePb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, undeadServicePb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	undeadServicePb.Metadata.UID = "undead_service"
	undeadServicePb.Metadata.DeletionTimestampNS = 0
	etcdServices[2] = undeadServicePb

	deletedUndeadServicePb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, deletedUndeadServicePb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deletedUndeadServicePb.Metadata.UID = "undead_service"
	deletedUndeadServicePb.Metadata.DeletionTimestampNS = 10

	mockMds.
		EXPECT().
		GetServices().
		Return(etcdServices, nil)

	mockMds.
		EXPECT().
		UpdateService(deletedUndeadServicePb).
		Return(nil)

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	isLeader := true
	mh, err := controllers.NewMetadataHandlerWithClock(mockMds, &isLeader, clock)
	assert.Nil(t, err)

	mh.SyncServiceData(&sList)
}

func TestSyncServicesData_NotLeader(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	services := make([]v1.Service, 1)
	// Create endpoints object.
	creationTime := metav1.Unix(0, 4)
	md := metav1.ObjectMeta{
		Name:              "kubernetes",
		Namespace:         "default",
		UID:               "active_service",
		ResourceVersion:   "1",
		CreationTimestamp: creationTime,
	}

	activeService := v1.Service{
		ObjectMeta: md,
	}
	services[0] = activeService

	sList := v1.ServiceList{
		Items: services,
	}

	etcdServices := make([](*metadatapb.Service), 3)
	activeServicePb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, activeServicePb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	activeServicePb.Metadata.UID = "active_service"
	activeServicePb.Metadata.DeletionTimestampNS = 0
	etcdServices[0] = activeServicePb

	deadServicePb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, deadServicePb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	deadServicePb.Metadata.UID = "dead_service"
	deadServicePb.Metadata.DeletionTimestampNS = 5
	etcdServices[1] = deadServicePb

	undeadServicePb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, undeadServicePb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	undeadServicePb.Metadata.UID = "undead_service"
	undeadServicePb.Metadata.DeletionTimestampNS = 0
	etcdServices[2] = undeadServicePb

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	isLeader := false
	mh, err := controllers.NewMetadataHandlerWithClock(mockMds, &isLeader, clock)
	assert.Nil(t, err)

	mh.SyncServiceData(&sList)
}
