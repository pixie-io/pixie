package controllers_test

import (
	"errors"
	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"testing"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
)

const endpointsPb = `
subsets {
  addresses {
    ip: "127.0.0.1"
    hostname: "host"
    node_name: "this-is-a-node"
    target_ref {
      kind: "pod"
      namespace: "pl"
    }
  }
  addresses {
    ip: "127.0.0.2"
    hostname: "host-2"
    node_name: "node-a"
  }
  not_ready_addresses {
    ip: "127.0.0.3"
    hostname: "host-3"
    node_name: "node-b"
  }
  ports {
    name: "endpt"
    port: 10,
    protocol: 1
  }
  ports {
    name: "abcd"
    port: 500,
    protocol: 1
  }
}
metadata {
  name: "object_md"
  namespace: "a_namespace"
  uid: "ijkl"
  resource_version: "1"
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
  owner_references {
    kind: "pod"
    name: "test"
    uid: "abcd"
  }
}
`

const servicePb = `
metadata {
	name: "object_md"
	namespace: "a_namespace"
	uid: "ijkl"
	resource_version: "1",
	cluster_name: "a_cluster",
	owner_references {
	  kind: "pod"
	  name: "test"
	  uid: "abcd"
	}
	creation_timestamp_ns: 4
	deletion_timestamp_ns: 6
}
spec {
	cluster_ip: "127.0.0.1"
	external_ips: "127.0.0.2"
	external_ips: "127.0.0.3"
	load_balancer_ip: "127.0.0.4"
	external_name: "hello"
	external_traffic_policy: 1
	ports {
		name: "endpt"
		port: 10
		protocol: 1
		node_port: 20
	}
	ports {
		name: "another_port"
		port: 50
		protocol: 1
		node_port: 60
	}
	type: 1
}
`

const podPb = `
metadata {
	name: "object_md"
	uid: "ijkl"
	resource_version: "1",
	cluster_name: "a_cluster",
	owner_references {
	  kind: "pod"
	  name: "test"
	  uid: "abcd"
	}
	creation_timestamp_ns: 4
	deletion_timestamp_ns: 6
}
status {
	message: "this is message"
	phase: 2
	conditions: 2
}
spec {
	node_name: "test"
	hostname: "hostname"
	dns_policy: 2
}
`

func TestObjectToEndpointsProto(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(endpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	updatePb := &messagespb.MetadataUpdateInfo_ResourceUpdate{
		Uid:  "ijkl",
		Name: "object_md",
		Type: messagespb.SERVICE,
	}
	update, err := updatePb.Marshal()

	mockMds.
		EXPECT().
		UpdateEndpoints(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"host", "host-2"}).
		Return(&[]string{"agent-1", "agent-2"}, nil)

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-1", string(update)).
		Return(nil)

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-2", string(update)).
		Return(nil)

	// Create endpoints object.
	or := v1.ObjectReference{
		Kind:      "pod",
		Namespace: "pl",
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

	mh.ProcessNextAgentUpdate()
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

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"host", "host-2"}).
		Return(nil, nil)

	// Create endpoints object.
	or := v1.ObjectReference{
		Kind:      "pod",
		Namespace: "pl",
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

	mh.ProcessNextAgentUpdate()
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

	updatePb := &messagespb.MetadataUpdateInfo_ResourceUpdate{
		Uid:  "ijkl",
		Name: "object_md",
		Type: messagespb.SERVICE,
	}
	update, err := updatePb.Marshal()

	mockMds.
		EXPECT().
		UpdateEndpoints(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"host", "host-2"}).
		Return(&[]string{"agent-1", "agent-2"}, nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"host-2"}).
		Return(&[]string{"agent-3"}, nil)

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-1", string(update)).
		Return(nil)

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-2", string(update)).
		Return(errors.New("Could not add to agent queue"))

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-3", string(update)).
		Return(nil)

	// Create endpoints object.
	or := v1.ObjectReference{
		Kind:      "pod",
		Namespace: "pl",
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

	mh.ProcessNextAgentUpdate()
	mh.ProcessNextAgentUpdate()
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

	mockMds.
		EXPECT().
		UpdateService(expectedPb).
		Return(nil)

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
	if err := proto.UnmarshalText(podPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	updatePb := &messagespb.MetadataUpdateInfo_ResourceUpdate{
		Uid:  "ijkl",
		Name: "object_md",
		Type: messagespb.POD,
	}

	update, err := updatePb.Marshal()

	mockMds.
		EXPECT().
		UpdatePod(expectedPb).
		Return(nil)

	mockMds.
		EXPECT().
		GetAgentsForHostnames(&[]string{"test"}).
		Return(&[]string{"agent-1"}, nil)

	mockMds.
		EXPECT().
		AddToAgentUpdateQueue("agent-1", string(update)).
		Return(nil)

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

	status := v1.PodStatus{
		Message:    "this is message",
		Phase:      v1.PodRunning,
		Conditions: conditions,
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
