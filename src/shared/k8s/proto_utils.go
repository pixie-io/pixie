package k8s

import (
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	types "k8s.io/apimachinery/pkg/types"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
)

var dnsPolicyObjToPbMap = map[v1.DNSPolicy]metadatapb.DNSPolicy{
	v1.DNSDefault:                 metadatapb.DEFAULT,
	v1.DNSNone:                    metadatapb.NONE,
	v1.DNSClusterFirst:            metadatapb.CLUSTER_FIRST,
	v1.DNSClusterFirstWithHostNet: metadatapb.CLUSTER_FIRST_WITH_HOST_NET,
}

var dnsPolicyPbToObjMap = map[metadatapb.DNSPolicy]v1.DNSPolicy{
	metadatapb.DEFAULT:                     v1.DNSDefault,
	metadatapb.NONE:                        v1.DNSNone,
	metadatapb.CLUSTER_FIRST:               v1.DNSClusterFirst,
	metadatapb.CLUSTER_FIRST_WITH_HOST_NET: v1.DNSClusterFirstWithHostNet,
}

var podPhaseObjToPbMap = map[v1.PodPhase]metadatapb.PodPhase{
	v1.PodPending:   metadatapb.PENDING,
	v1.PodRunning:   metadatapb.RUNNING,
	v1.PodSucceeded: metadatapb.SUCCEEDED,
	v1.PodUnknown:   metadatapb.PHASE_UNKNOWN,
}

var podPhasePbToObjMap = map[metadatapb.PodPhase]v1.PodPhase{
	metadatapb.PENDING:       v1.PodPending,
	metadatapb.RUNNING:       v1.PodRunning,
	metadatapb.SUCCEEDED:     v1.PodSucceeded,
	metadatapb.PHASE_UNKNOWN: v1.PodUnknown,
}

var podConditionObjToPbMap = map[v1.PodConditionType]metadatapb.PodConditionType{
	v1.ContainersReady: metadatapb.CONTAINERS_READY,
	v1.PodInitialized:  metadatapb.INITIALIZED,
	v1.PodReady:        metadatapb.READY,
	v1.PodScheduled:    metadatapb.POD_SCHEDULED,
}

var podConditionPbToObjMap = map[metadatapb.PodConditionType]v1.PodConditionType{
	metadatapb.CONTAINERS_READY: v1.ContainersReady,
	metadatapb.INITIALIZED:      v1.PodInitialized,
	metadatapb.READY:            v1.PodReady,
	metadatapb.POD_SCHEDULED:    v1.PodScheduled,
}

// OwnerReferenceToProto converts an OwnerReference into a proto.
func OwnerReferenceToProto(o *metav1.OwnerReference) (*metadatapb.OwnerReference, error) {
	oPb := &metadatapb.OwnerReference{
		Kind: o.Kind,
		Name: o.Name,
		Uid:  string(o.UID),
	}
	return oPb, nil
}

// OwnerReferenceFromProto converts a proto message to an OwnerReference.
func OwnerReferenceFromProto(pb *metadatapb.OwnerReference) (*metav1.OwnerReference, error) {
	obj := &metav1.OwnerReference{
		Kind: pb.Kind,
		Name: pb.Name,
		UID:  types.UID(pb.Uid),
	}
	return obj, nil
}

// ObjectMetadataToProto converts an ObjectMeta into a proto.
func ObjectMetadataToProto(o *metav1.ObjectMeta) (*metadatapb.ObjectMetadata, error) {
	ownerRefs := make([]*metadatapb.OwnerReference, len(o.OwnerReferences))

	for i, ref := range o.OwnerReferences {
		refPb, err := OwnerReferenceToProto(&ref)
		if err != nil {
			return nil, nil
		}
		ownerRefs[i] = refPb
	}

	oPb := &metadatapb.ObjectMetadata{
		Name:                o.Name,
		Namespace:           o.Namespace,
		Uid:                 string(o.UID),
		ResourceVersion:     o.ResourceVersion,
		ClusterName:         o.ClusterName,
		OwnerReferences:     ownerRefs,
		Labels:              o.Labels,
		CreationTimestampNs: o.CreationTimestamp.UnixNano(),
	}

	if o.DeletionTimestamp != nil {
		oPb.DeletionTimestampNs = o.DeletionTimestamp.UnixNano()
	}

	return oPb, nil
}

// ObjectMetadataFromProto converts a proto message to an ObjectMeta.
func ObjectMetadataFromProto(pb *metadatapb.ObjectMetadata) (*metav1.ObjectMeta, error) {
	var delTime metav1.Time
	var creationTime metav1.Time
	if pb.DeletionTimestampNs != 0 {
		delTime = metav1.Unix(0, pb.DeletionTimestampNs)
	}
	if pb.CreationTimestampNs != 0 {
		creationTime = metav1.Unix(0, pb.CreationTimestampNs)
	}

	ownerRefs := make([]metav1.OwnerReference, len(pb.OwnerReferences))
	for i, refPb := range pb.OwnerReferences {
		ref, err := OwnerReferenceFromProto(refPb)
		if err != nil {
			return &metav1.ObjectMeta{}, nil
		}
		ownerRefs[i] = *ref
	}

	o := &metav1.ObjectMeta{
		Name:              pb.Name,
		Namespace:         pb.Namespace,
		UID:               types.UID(pb.Uid),
		ResourceVersion:   pb.ResourceVersion,
		ClusterName:       pb.ClusterName,
		Labels:            pb.Labels,
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
		OwnerReferences:   ownerRefs,
	}
	return o, nil
}

// PodSpecToProto converts an PodSpec into a proto.
func PodSpecToProto(ps *v1.PodSpec) (*metadatapb.PodSpec, error) {
	psPb := &metadatapb.PodSpec{
		NodeSelector:      ps.NodeSelector,
		NodeName:          ps.NodeName,
		Hostname:          ps.Hostname,
		Subdomain:         ps.Subdomain,
		PriorityClassName: ps.PriorityClassName,
		DnsPolicy:         dnsPolicyObjToPbMap[ps.DNSPolicy],
	}
	if ps.Priority != nil {
		psPb.Priority = *ps.Priority
	}

	return psPb, nil
}

// PodSpecFromProto converts a proto message to a PodSpec.
func PodSpecFromProto(pb *metadatapb.PodSpec) (*v1.PodSpec, error) {
	ps := &v1.PodSpec{
		NodeSelector:      pb.NodeSelector,
		NodeName:          pb.NodeName,
		Hostname:          pb.Hostname,
		Subdomain:         pb.Subdomain,
		PriorityClassName: pb.PriorityClassName,
		DNSPolicy:         dnsPolicyPbToObjMap[pb.DnsPolicy],
		Priority:          &pb.Priority,
	}

	return ps, nil
}

// PodStatusToProto converts an PodStatus into a proto.
func PodStatusToProto(ps *v1.PodStatus) (*metadatapb.PodStatus, error) {
	conditions := make([]metadatapb.PodConditionType, len(ps.Conditions))
	for i, c := range ps.Conditions {
		conditions[i] = podConditionObjToPbMap[c.Type]
	}

	psPb := &metadatapb.PodStatus{
		Message:    ps.Message,
		Reason:     ps.Reason,
		HostIp:     ps.HostIP,
		PodIp:      ps.PodIP,
		Phase:      podPhaseObjToPbMap[ps.Phase],
		Conditions: conditions,
	}

	return psPb, nil
}

// PodStatusFromProto converts a proto message to a PodStatus.
func PodStatusFromProto(pb *metadatapb.PodStatus) (*v1.PodStatus, error) {
	conditions := make([]v1.PodCondition, len(pb.Conditions))
	for i, c := range pb.Conditions {
		podCond := v1.PodCondition{
			Type: podConditionPbToObjMap[c],
		}
		conditions[i] = podCond
	}

	ps := &v1.PodStatus{
		Message:    pb.Message,
		Reason:     pb.Reason,
		HostIP:     pb.HostIp,
		PodIP:      pb.PodIp,
		Phase:      podPhasePbToObjMap[pb.Phase],
		Conditions: conditions,
	}

	return ps, nil
}
