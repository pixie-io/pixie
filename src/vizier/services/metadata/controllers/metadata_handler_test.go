package controllers_test

import (
	"errors"
	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"sync"
	"testing"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
)

func TestObjectToEndpointsProto(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(endpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	updatePb := &metadatapb.ResourceUpdate{
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

	update, err := updatePb.Marshal()

	mockMds.
		EXPECT().
		UpdateEndpoints(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"this-is-a-node", "node-a"}).
		Return(&[]string{"agent-1", "agent-2"}, nil)

	var wg sync.WaitGroup
	wg.Add(2)
	defer wg.Wait()

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-1", string(update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return nil
		})

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-2", string(update)).
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
		IP:       "127.0.0.2",
		Hostname: "host-2",
		NodeName: &nodeName2,
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

	mh, err := controllers.NewMetadataHandler(mockMds)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "endpoints"}
	ch <- msg

	more := mh.ProcessNextAgentUpdate()
	assert.Equal(t, true, more)
}

func TestNoHostnameResolvedProto(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(endpointsPb, expectedPb); err != nil {
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
		GetAgentsForHostnames(&[]string{"this-is-a-node", "node-a"}).
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
		IP:       "127.0.0.2",
		Hostname: "host-2",
		NodeName: &nodeName2,
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

	mh, err := controllers.NewMetadataHandler(mockMds)
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
	if err := proto.UnmarshalText(endpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	refs := make([]*metadatapb.ObjectReference, 1)
	refs[0] = &metadatapb.ObjectReference{
		Kind:      "Pod",
		Namespace: "pl",
		UID:       "abcd",
	}

	updatePb := &metadatapb.ResourceUpdate{
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
	update, err := updatePb.Marshal()

	mockMds.
		EXPECT().
		UpdateEndpoints(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"this-is-a-node", "node-a"}).
		Return(&[]string{"agent-1", "agent-2"}, nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"node-a"}).
		Return(&[]string{"agent-3"}, nil)

	var wg sync.WaitGroup
	wg.Add(3)
	defer wg.Wait()

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-1", string(update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return nil
		})

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-2", string(update)).
		DoAndReturn(func(agent string, update string) error {
			wg.Done()
			return errors.New("Could not add to agent queue")
		})

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-3", string(update)).
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
		IP:       "127.0.0.2",
		Hostname: "host-2",
		NodeName: &nodeName2,
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

	mh, err := controllers.NewMetadataHandler(mockMds)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "endpoints"}
	ch <- msg

	more := mh.ProcessNextAgentUpdate()
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
	if err := proto.UnmarshalText(endpointsPb, expectedPb); err != nil {
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

	mh, err := controllers.NewMetadataHandler(mockMds)
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
	if err := proto.UnmarshalText(servicePb, expectedPb); err != nil {
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

	mh, err := controllers.NewMetadataHandler(mockMds)
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
	if err := proto.UnmarshalText(podPbWithContainers, expectedPb); err != nil {
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

	mh, err := controllers.NewMetadataHandler(mockMds)
	assert.Nil(t, err)

	ch := mh.GetChannel()
	msg := &controllers.K8sMessage{Object: &o, ObjectType: "pods"}
	ch <- msg

	mh.ProcessNextAgentUpdate()
}

func TestGetResourceUpdateFromPod(t *testing.T) {
	pod := &metadatapb.Pod{}
	if err := proto.UnmarshalText(podPbWithContainers, pod); err != nil {
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
	if err := proto.UnmarshalText(endpointsPb, ep); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	update := controllers.GetResourceUpdateFromEndpoints(ep)
	serviceUpdate := update.GetServiceUpdate()
	assert.NotNil(t, serviceUpdate)
	assert.Equal(t, "object_md", serviceUpdate.Name)
	assert.Equal(t, "ijkl", serviceUpdate.UID)
	assert.Equal(t, int64(4), serviceUpdate.StartTimestampNS)
	assert.Equal(t, int64(6), serviceUpdate.StopTimestampNS)
	assert.Equal(t, 1, len(serviceUpdate.PodIDs))
	assert.Equal(t, "abcd", serviceUpdate.PodIDs[0])
}

func TestGetContainerResourceUpdatesFromPod(t *testing.T) {
	pod := &metadatapb.Pod{}
	if err := proto.UnmarshalText(podPbWithContainers, pod); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	updates := controllers.GetContainerResourceUpdatesFromPod(pod)
	assert.Equal(t, 1, len(updates))
	cUpdate := updates[0].GetContainerUpdate()
	assert.NotNil(t, cUpdate)
	assert.Equal(t, "container1", cUpdate.Name)
	assert.Equal(t, "test", cUpdate.CID)

}
