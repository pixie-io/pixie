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

package k8s_test

import (
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/types"

	"px.dev/pixie/src/shared/k8s"
	"px.dev/pixie/src/shared/k8s/metadatapb"
)

const ownerReferencePb = `
kind: "pod"
name: "abcd"
uid: "efgh"
`

const objectMetadataPb = `
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
owner_references {
  kind: "pod"
  name: "another_test"
  uid: "efgh"
}
labels {
  key: "test"
  value: "value"
}
labels {
  key: "label"
  value: "another_value"
}
annotations {
	key: "annotation1"
	value: "hello"
}
annotations {
	key: "somethingElse"
	value: "hello2"
}
creation_timestamp_ns: 4
deletion_timestamp_ns: 6
`

const objectMetadataMissingClusterPb = `
name: "object_md"
namespace: "a_namespace"
uid: "ijkl"
resource_version: "1",
owner_references {
  kind: "pod"
  name: "test"
  uid: "abcd"
}
owner_references {
  kind: "pod"
  name: "another_test"
  uid: "efgh"
}
creation_timestamp_ns: 4
deletion_timestamp_ns: 6
`

const podSpecPb = `
node_name: "test"
hostname: "hostname"
dns_policy: 2
`

const podStatusPb = `
message: "this is message"
phase: 2
conditions {
	type: 2
	status: 1
}
qos_class: 3
container_statuses {
   name: "test_container_2"
   container_id: "test_id_2"
   container_state: 1
   start_timestamp_ns: 4
}
container_statuses {
   name: "test_container"
   container_id: "test_id"
   container_state: 3
}

`

const podPb = `
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
status {
	message: "this is message"
	phase: 2
	conditions {
		type: 2
		status: 2
	}
	container_statuses {
	   name: "test_container_2"
	   container_id: "test_id_2"
	   container_state: 1
	   start_timestamp_ns: 4
	}
	container_statuses {
	   name: "test_container"
	   container_id: "test_id"
	   container_state: 3
	}
}
spec {
	node_name: "test"
	hostname: "hostname"
	dns_policy: 2
}
`

const namespacePb = `
metadata {
	name: "a_namespace"
	namespace: "a_namespace"
	uid: "ijkl"
	resource_version: "1",
	cluster_name: "a_cluster",
	creation_timestamp_ns: 4
	deletion_timestamp_ns: 6
	owner_references {
	  kind: "cluster"
	  name: "test"
	  uid: "abcd"
	}
}
`

const objectReferencePb = `
kind: "pod"
namespace: "pl"
name: "pod-abc"
uid: "abcd"
`

const endpointPortPb = `
name: "endpt"
port: 10,
protocol: 1
`

const endpointAddrPb = `
ip: "127.0.0.1"
hostname: "host"
node_name: "this-is-a-node"
target_ref {
	kind: "pod"
	namespace: "pl"
}
`

const endpointSubsetPb = `
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
`

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

const servicePortPb = `
name: "endpt"
port: 10
protocol: 1
node_port: 20
`

const serviceSpecPb = `
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

const waitingContainerStatusPb = `
name: "test_container"
container_id: "test_id"
container_state: 3
reason: "reason"
`

const runningContainerStatusPb = `
name: "test_container"
container_id: "test_id"
container_state: 1
start_timestamp_ns: 4
`

const terminatedContainerStatusPb = `
name: "test_container"
container_id: "test_id"
container_state: 2
start_timestamp_ns: 4
stop_timestamp_ns: 6
`

const nodePb = `
metadata {
	name: "some_node"
	uid: "12"
	resource_version: "1",
	creation_timestamp_ns: 4
	owner_references: {}
}
status {
	addresses {
		address: "10.32.0.77"
		type: 3
	}
	addresses {
		address: "34.82.242.42"
		type: 2
	}
	phase: 2
}
spec {
	pod_cidr: "10.60.4.0/24"
	pod_cidrs: "10.60.4.0/24"
}
`

func TestOwnerReferenceToProto(t *testing.T) {
	o := metav1.OwnerReference{
		Kind: "pod",
		Name: "abcd",
		UID:  "efgh",
	}

	oPb, err := k8s.OwnerReferenceToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.OwnerReference{}
	if err := proto.UnmarshalText(ownerReferencePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestOwnerReferenceFromProto(t *testing.T) {
	oPb := &metadatapb.OwnerReference{}
	if err := proto.UnmarshalText(ownerReferencePb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.OwnerReferenceFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "pod", obj.Kind)
	assert.Equal(t, "abcd", obj.Name)
	assert.Equal(t, types.UID("efgh"), obj.UID)
}

func TestObjectMetadataToProto(t *testing.T) {
	labels := make(map[string]string)
	labels["test"] = "value"
	labels["label"] = "another_value"

	annotations := map[string]string{
		"annotation1":   "hello",
		"somethingElse": "hello2",
	}

	ownerRefs := make([]metav1.OwnerReference, 2)
	ownerRefs[0] = metav1.OwnerReference{
		Kind: "pod",
		Name: "test",
		UID:  "abcd",
	}

	ownerRefs[1] = metav1.OwnerReference{
		Kind: "pod",
		Name: "another_test",
		UID:  "efgh",
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	o := metav1.ObjectMeta{
		Name:              "object_md",
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		ClusterName:       "a_cluster",
		OwnerReferences:   ownerRefs,
		Labels:            labels,
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
		Annotations:       annotations,
	}

	oPb, err := k8s.ObjectMetadataToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.ObjectMetadata{}
	if err := proto.UnmarshalText(objectMetadataPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestObjectMetadataMissingClusterToProto(t *testing.T) {
	ownerRefs := make([]metav1.OwnerReference, 2)
	ownerRefs[0] = metav1.OwnerReference{
		Kind: "pod",
		Name: "test",
		UID:  "abcd",
	}

	ownerRefs[1] = metav1.OwnerReference{
		Kind: "pod",
		Name: "another_test",
		UID:  "efgh",
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	o := metav1.ObjectMeta{
		Name:              "object_md",
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		OwnerReferences:   ownerRefs,
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
	}

	oPb, err := k8s.ObjectMetadataToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.ObjectMetadata{}
	if err := proto.UnmarshalText(objectMetadataMissingClusterPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestObjectMetadataFromProto(t *testing.T) {
	oPb := &metadatapb.ObjectMetadata{}
	if err := proto.UnmarshalText(objectMetadataPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.ObjectMetadataFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)

	assert.Equal(t, "object_md", obj.Name)
	assert.Equal(t, "a_namespace", obj.Namespace)
	assert.Equal(t, types.UID("ijkl"), obj.UID)
	assert.Equal(t, "1", obj.ResourceVersion)
	assert.Equal(t, "a_cluster", obj.ClusterName)
	assert.Equal(t, 2, len(obj.Labels))
	assert.Equal(t, "value", obj.Labels["test"])
	assert.Equal(t, "another_value", obj.Labels["label"])
	assert.Equal(t, true, obj.CreationTimestamp.Equal(&creationTime))
	assert.Equal(t, true, obj.DeletionTimestamp.Equal(&delTime))
	assert.Equal(t, 2, len(obj.OwnerReferences))
	assert.Equal(t, "test", obj.OwnerReferences[0].Name)
}

func TestPodSpecToProto(t *testing.T) {
	o := v1.PodSpec{
		NodeName:  "test",
		Hostname:  "hostname",
		DNSPolicy: v1.DNSClusterFirst,
	}

	oPb, err := k8s.PodSpecToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.PodSpec{}
	if err := proto.UnmarshalText(podSpecPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestPodSpecFromProto(t *testing.T) {
	oPb := &metadatapb.PodSpec{}
	if err := proto.UnmarshalText(podSpecPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.PodSpecFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "test", obj.NodeName)
	assert.Equal(t, "hostname", obj.Hostname)
	assert.Equal(t, v1.DNSClusterFirst, obj.DNSPolicy)
}

func TestPodStatusToProto(t *testing.T) {
	conditions := make([]v1.PodCondition, 1)
	conditions[0] = v1.PodCondition{
		Type:   v1.PodReady,
		Status: v1.ConditionTrue,
	}

	containers := make([]v1.ContainerStatus, 2)
	startTime := metav1.Unix(0, 4)
	runningState := v1.ContainerStateRunning{
		StartedAt: startTime,
	}
	containers[0] = v1.ContainerStatus{
		Name:        "test_container_2",
		ContainerID: "test_id_2",
		State: v1.ContainerState{
			Running: &runningState,
		},
	}
	waitingState := v1.ContainerStateWaiting{}
	containers[1] = v1.ContainerStatus{
		Name:        "test_container",
		ContainerID: "test_id",
		State: v1.ContainerState{
			Waiting: &waitingState,
		},
	}

	o := v1.PodStatus{
		Message:           "this is message",
		Phase:             v1.PodRunning,
		Conditions:        conditions,
		QOSClass:          v1.PodQOSBestEffort,
		ContainerStatuses: containers,
	}

	oPb, err := k8s.PodStatusToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.PodStatus{}
	if err := proto.UnmarshalText(podStatusPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestPodStatusFromProto(t *testing.T) {
	oPb := &metadatapb.PodStatus{}
	if err := proto.UnmarshalText(podStatusPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.PodStatusFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "this is message", obj.Message)
	assert.Equal(t, v1.PodRunning, obj.Phase)
	assert.Equal(t, 1, len(obj.Conditions))
	assert.Equal(t, v1.PodReady, obj.Conditions[0].Type)
	assert.Equal(t, v1.PodQOSBestEffort, obj.QOSClass)
}

func TestPodToProto(t *testing.T) {
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

	conditions := make([]v1.PodCondition, 1)
	conditions[0] = v1.PodCondition{
		Type:   v1.PodReady,
		Status: v1.ConditionFalse,
	}

	containers := make([]v1.ContainerStatus, 2)
	startTime := metav1.Unix(0, 4)
	runningState := v1.ContainerStateRunning{
		StartedAt: startTime,
	}
	containers[0] = v1.ContainerStatus{
		Name:        "test_container_2",
		ContainerID: "test_id_2",
		State: v1.ContainerState{
			Running: &runningState,
		},
	}
	waitingState := v1.ContainerStateWaiting{}
	containers[1] = v1.ContainerStatus{
		Name:        "test_container",
		ContainerID: "test_id",
		State: v1.ContainerState{
			Waiting: &waitingState,
		},
	}

	status := v1.PodStatus{
		Message:           "this is message",
		Phase:             v1.PodRunning,
		Conditions:        conditions,
		ContainerStatuses: containers,
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

	oPb, err := k8s.PodToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(podPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestPodFromProto(t *testing.T) {
	oPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(podPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.PodFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "object_md", obj.ObjectMeta.Name)
	assert.Equal(t, "this is message", obj.Status.Message)
	assert.Equal(t, "test", obj.Spec.NodeName)
}

func TestNamespaceToProto(t *testing.T) {
	ownerRefs := make([]metav1.OwnerReference, 1)
	ownerRefs[0] = metav1.OwnerReference{
		Kind: "cluster",
		Name: "test",
		UID:  "abcd",
	}

	delTime := metav1.Unix(0, 6)
	creationTime := metav1.Unix(0, 4)
	metadata := metav1.ObjectMeta{
		Name:              "a_namespace",
		Namespace:         "a_namespace",
		UID:               "ijkl",
		ResourceVersion:   "1",
		ClusterName:       "a_cluster",
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
		OwnerReferences:   ownerRefs,
	}

	o := v1.Namespace{
		ObjectMeta: metadata,
	}

	oPb, err := k8s.NamespaceToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.Namespace{}
	if err := proto.UnmarshalText(namespacePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestObjectReferenceToProto(t *testing.T) {
	o := v1.ObjectReference{
		Kind:      "pod",
		Namespace: "pl",
		Name:      "pod-abc",
		UID:       types.UID("abcd"),
	}

	oPb, err := k8s.ObjectReferenceToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.ObjectReference{}
	if err := proto.UnmarshalText(objectReferencePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestObjectReferenceFromProto(t *testing.T) {
	oPb := &metadatapb.ObjectReference{}
	if err := proto.UnmarshalText(objectReferencePb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.ObjectReferenceFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "pod", obj.Kind)
	assert.Equal(t, "pl", obj.Namespace)
	assert.Equal(t, "pod-abc", obj.Name)
	assert.Equal(t, "abcd", string(obj.UID))
}

func TestEndpointPortToProto(t *testing.T) {
	o := v1.EndpointPort{
		Name:     "endpt",
		Port:     10,
		Protocol: v1.ProtocolTCP,
	}

	oPb, err := k8s.EndpointPortToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.EndpointPort{}
	if err := proto.UnmarshalText(endpointPortPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestEndpointPortFromProto(t *testing.T) {
	oPb := &metadatapb.EndpointPort{}
	if err := proto.UnmarshalText(endpointPortPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.EndpointPortFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "endpt", obj.Name)
	assert.Equal(t, int32(10), obj.Port)
	assert.Equal(t, v1.ProtocolTCP, obj.Protocol)
}

func TestEndpointAddressToProto(t *testing.T) {
	or := v1.ObjectReference{
		Kind:      "pod",
		Namespace: "pl",
	}

	nodeName := "this-is-a-node"
	o := v1.EndpointAddress{
		IP:        "127.0.0.1",
		Hostname:  "host",
		NodeName:  &nodeName,
		TargetRef: &or,
	}

	oPb, err := k8s.EndpointAddressToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.EndpointAddress{}
	if err := proto.UnmarshalText(endpointAddrPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestEndpointAddressFromProto(t *testing.T) {
	oPb := &metadatapb.EndpointAddress{}
	if err := proto.UnmarshalText(endpointAddrPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.EndpointAddressFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "127.0.0.1", obj.IP)
	assert.Equal(t, "host", obj.Hostname)
	assert.Equal(t, "this-is-a-node", *obj.NodeName)
	assert.Equal(t, "pod", obj.TargetRef.Kind)
}

func TestEndpointSubsetToProto(t *testing.T) {
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

	o := v1.EndpointSubset{
		Addresses:         addrs,
		NotReadyAddresses: notReadyAddrs,
		Ports:             ports,
	}

	oPb, err := k8s.EndpointSubsetToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.EndpointSubset{}
	if err := proto.UnmarshalText(endpointSubsetPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestEndpointSubsetsFromProto(t *testing.T) {
	oPb := &metadatapb.EndpointSubset{}
	if err := proto.UnmarshalText(endpointSubsetPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.EndpointSubsetFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, 2, len(obj.Addresses))
	assert.Equal(t, "127.0.0.1", obj.Addresses[0].IP)
	assert.Equal(t, "127.0.0.2", obj.Addresses[1].IP)
	assert.Equal(t, "pod", obj.Addresses[0].TargetRef.Kind)
	assert.Equal(t, 1, len(obj.NotReadyAddresses))
	assert.Equal(t, "127.0.0.3", obj.NotReadyAddresses[0].IP)
	assert.Equal(t, 2, len(obj.Ports))
	assert.Equal(t, "endpt", obj.Ports[0].Name)
	assert.Equal(t, "abcd", obj.Ports[1].Name)
}

func TestEndpointsToProto(t *testing.T) {
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

	oPb, err := k8s.EndpointsToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(endpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestEndpointsFromProto(t *testing.T) {
	oPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(endpointsPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	e, err := k8s.EndpointsFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "object_md", e.ObjectMeta.Name)
	assert.Equal(t, 1, len(e.Subsets))
	assert.Equal(t, 2, len(e.Subsets[0].Addresses))
}

func TestServicePortToProto(t *testing.T) {
	o := v1.ServicePort{
		Name:     "endpt",
		Port:     10,
		Protocol: v1.ProtocolTCP,
		NodePort: 20,
	}

	oPb, err := k8s.ServicePortToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.ServicePort{}
	if err := proto.UnmarshalText(servicePortPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestServicePortFromProto(t *testing.T) {
	oPb := &metadatapb.ServicePort{}
	if err := proto.UnmarshalText(servicePortPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.ServicePortFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "endpt", obj.Name)
	assert.Equal(t, int32(10), obj.Port)
	assert.Equal(t, v1.ProtocolTCP, obj.Protocol)
	assert.Equal(t, int32(20), obj.NodePort)
}

func TestServiceSpecToProto(t *testing.T) {
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

	o := v1.ServiceSpec{
		ClusterIP:             "127.0.0.1",
		LoadBalancerIP:        "127.0.0.4",
		ExternalName:          "hello",
		ExternalTrafficPolicy: v1.ServiceExternalTrafficPolicyTypeLocal,
		Type:                  v1.ServiceTypeExternalName,
		Ports:                 ports,
		ExternalIPs:           externalIPs,
	}

	oPb, err := k8s.ServiceSpecToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.ServiceSpec{}
	if err := proto.UnmarshalText(serviceSpecPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestServiceSpecFromProto(t *testing.T) {
	oPb := &metadatapb.ServiceSpec{}
	if err := proto.UnmarshalText(serviceSpecPb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.ServiceSpecFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "127.0.0.1", obj.ClusterIP)
	assert.Equal(t, "127.0.0.4", obj.LoadBalancerIP)
	assert.Equal(t, "hello", obj.ExternalName)
	assert.Equal(t, v1.ServiceExternalTrafficPolicyTypeLocal, obj.ExternalTrafficPolicy)
	assert.Equal(t, v1.ServiceTypeExternalName, obj.Type)
	assert.Equal(t, 2, len(obj.Ports))
	assert.Equal(t, "endpt", obj.Ports[0].Name)
	assert.Equal(t, 2, len(obj.ExternalIPs))
	assert.Equal(t, "127.0.0.2", obj.ExternalIPs[0])
}

func TestServiceToProto(t *testing.T) {
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

	oPb, err := k8s.ServiceToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(servicePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestServiceFromProto(t *testing.T) {
	oPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(servicePb, oPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	obj, err := k8s.ServiceFromProto(oPb)
	require.NoError(t, err, "must not have an error")

	assert.Equal(t, "object_md", obj.ObjectMeta.Name)
	assert.Equal(t, "hello", obj.Spec.ExternalName)
}

func TestContainerStatusToProtoWaiting(t *testing.T) {
	waitingState := v1.ContainerStateWaiting{
		Reason: "reason",
	}

	state := v1.ContainerState{
		Waiting: &waitingState,
	}

	o := v1.ContainerStatus{
		Name:        "test_container",
		ContainerID: "test_id",
		State:       state,
	}

	oPb, err := k8s.ContainerStatusToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.ContainerStatus{}
	if err := proto.UnmarshalText(waitingContainerStatusPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestContainerStatusToProtoRunning(t *testing.T) {
	startTime := metav1.Unix(0, 4)
	runningState := v1.ContainerStateRunning{
		StartedAt: startTime,
	}

	state := v1.ContainerState{
		Running: &runningState,
	}

	o := v1.ContainerStatus{
		Name:        "test_container",
		ContainerID: "test_id",
		State:       state,
	}

	oPb, err := k8s.ContainerStatusToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.ContainerStatus{}
	if err := proto.UnmarshalText(runningContainerStatusPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestContainerStatusToProtoTerminated(t *testing.T) {
	startTime := metav1.Unix(0, 4)
	stopTime := metav1.Unix(0, 6)
	terminatedState := v1.ContainerStateTerminated{
		StartedAt:  startTime,
		FinishedAt: stopTime,
	}

	state := v1.ContainerState{
		Terminated: &terminatedState,
	}

	o := v1.ContainerStatus{
		Name:        "test_container",
		ContainerID: "test_id",
		State:       state,
	}

	oPb, err := k8s.ContainerStatusToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.ContainerStatus{}
	if err := proto.UnmarshalText(terminatedContainerStatusPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestNodeToProto(t *testing.T) {
	oRefs := []metav1.OwnerReference{
		{
			Kind: "",
			Name: "",
			UID:  "",
		},
	}

	creationTime := metav1.Unix(0, 4)
	metadata := metav1.ObjectMeta{
		Name:              "some_node",
		Namespace:         "",
		UID:               "12",
		ResourceVersion:   "1",
		ClusterName:       "",
		CreationTimestamp: creationTime,
		OwnerReferences:   oRefs,
	}

	addresses := []v1.NodeAddress{
		{
			Type:    v1.NodeInternalIP,
			Address: "10.32.0.77",
		},
		{
			Type:    v1.NodeExternalIP,
			Address: "34.82.242.42",
		},
	}

	status := v1.NodeStatus{
		Phase:     v1.NodeRunning,
		Addresses: addresses,
	}

	spec := v1.NodeSpec{
		PodCIDR:  "10.60.4.0/24",
		PodCIDRs: []string{"10.60.4.0/24"},
	}

	o := v1.Node{
		ObjectMeta: metadata,
		Status:     status,
		Spec:       spec,
	}

	oPb, err := k8s.NodeToProto(&o)
	require.NoError(t, err, "must not have an error")

	expectedPb := &metadatapb.Node{}
	if err := proto.UnmarshalText(nodePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}
