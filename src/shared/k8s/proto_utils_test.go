package k8s_test

import (
	"testing"

	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	types "k8s.io/apimachinery/pkg/types"

	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
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
conditions: 2
`

func TestOwnerReferenceToProto(t *testing.T) {
	o := metav1.OwnerReference{
		Kind: "pod",
		Name: "abcd",
		UID:  "efgh",
	}

	oPb, err := k8s.OwnerReferenceToProto(&o)
	assert.Nil(t, err, "must not have an error")

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
	assert.Nil(t, err, "must not have an error")

	assert.Equal(t, "pod", obj.Kind)
	assert.Equal(t, "abcd", obj.Name)
	assert.Equal(t, types.UID("efgh"), obj.UID)
}

func TestObjectMetadataToProto(t *testing.T) {
	labels := make(map[string]string)
	labels["test"] = "value"
	labels["label"] = "another_value"

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
	}

	oPb, err := k8s.ObjectMetadataToProto(&o)
	assert.Nil(t, err, "must not have an error")

	expectedPb := &metadatapb.ObjectMetadata{}
	if err := proto.UnmarshalText(objectMetadataPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, expectedPb, oPb)
}

func TestObjectMetadataMissingClusterToProto(t *testing.T) {
	var ownerRefs []metav1.OwnerReference
	or1 := metav1.OwnerReference{
		Kind: "pod",
		Name: "test",
		UID:  "abcd",
	}
	ownerRefs = append(ownerRefs, or1)

	or2 := metav1.OwnerReference{
		Kind: "pod",
		Name: "another_test",
		UID:  "efgh",
	}
	ownerRefs = append(ownerRefs, or2)

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
	assert.Nil(t, err, "must not have an error")

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
	assert.Nil(t, err, "must not have an error")

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
	assert.Nil(t, err, "must not have an error")

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
	assert.Nil(t, err, "must not have an error")

	assert.Equal(t, "test", obj.NodeName)
	assert.Equal(t, "hostname", obj.Hostname)
	assert.Equal(t, v1.DNSClusterFirst, obj.DNSPolicy)
}

func TestPodStatusToProto(t *testing.T) {
	conditions := make([]v1.PodCondition, 1)
	conditions[0] = v1.PodCondition{
		Type: v1.PodReady,
	}

	o := v1.PodStatus{
		Message:    "this is message",
		Phase:      v1.PodRunning,
		Conditions: conditions,
	}

	oPb, err := k8s.PodStatusToProto(&o)
	assert.Nil(t, err, "must not have an error")

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
	assert.Nil(t, err, "must not have an error")

	assert.Equal(t, "this is message", obj.Message)
	assert.Equal(t, v1.PodRunning, obj.Phase)
	assert.Equal(t, 1, len(obj.Conditions))
	assert.Equal(t, v1.PodReady, obj.Conditions[0].Type)
}
