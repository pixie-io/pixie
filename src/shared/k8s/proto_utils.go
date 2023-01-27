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

package k8s

import (
	"fmt"

	apps "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/types"
	"k8s.io/apimachinery/pkg/util/intstr"

	"px.dev/pixie/src/shared/k8s/metadatapb"
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
	v1.PodFailed:    metadatapb.FAILED,
	v1.PodUnknown:   metadatapb.PHASE_UNKNOWN,
}

var podPhasePbToObjMap = map[metadatapb.PodPhase]v1.PodPhase{
	metadatapb.PENDING:       v1.PodPending,
	metadatapb.RUNNING:       v1.PodRunning,
	metadatapb.SUCCEEDED:     v1.PodSucceeded,
	metadatapb.FAILED:        v1.PodFailed,
	metadatapb.PHASE_UNKNOWN: v1.PodUnknown,
	// Terminated is not a Kubernetes pod phase, because Kubernetes does not keep track
	// of pods that are terminated. It is useful for us to keep Terminated as a separate
	// pod phase since Pixie does track terminated pods.
	metadatapb.TERMINATED: v1.PodUnknown,
}

var podConditionTypeObjToPbMap = map[v1.PodConditionType]metadatapb.PodConditionType{
	v1.ContainersReady: metadatapb.CONTAINERS_READY,
	v1.PodInitialized:  metadatapb.INITIALIZED,
	v1.PodReady:        metadatapb.READY,
	v1.PodScheduled:    metadatapb.POD_SCHEDULED,
}

var podConditionTypePbToObjMap = map[metadatapb.PodConditionType]v1.PodConditionType{
	metadatapb.CONTAINERS_READY: v1.ContainersReady,
	metadatapb.INITIALIZED:      v1.PodInitialized,
	metadatapb.READY:            v1.PodReady,
	metadatapb.POD_SCHEDULED:    v1.PodScheduled,
}

var ipProtocolObjToPbMap = map[v1.Protocol]metadatapb.IPProtocol{
	v1.ProtocolTCP:  metadatapb.TCP,
	v1.ProtocolUDP:  metadatapb.UDP,
	v1.ProtocolSCTP: metadatapb.SCTP,
}

var ipProtocolPbToObjMap = map[metadatapb.IPProtocol]v1.Protocol{
	metadatapb.TCP:  v1.ProtocolTCP,
	metadatapb.UDP:  v1.ProtocolUDP,
	metadatapb.SCTP: v1.ProtocolSCTP,
}

var serviceTypeObjToPbMap = map[v1.ServiceType]metadatapb.ServiceType{
	v1.ServiceTypeClusterIP:    metadatapb.CLUSTER_IP,
	v1.ServiceTypeNodePort:     metadatapb.NODE_PORT,
	v1.ServiceTypeLoadBalancer: metadatapb.LOAD_BALANCER,
	v1.ServiceTypeExternalName: metadatapb.EXTERNAL_NAME,
}

var serviceTypePbToObjMap = map[metadatapb.ServiceType]v1.ServiceType{
	metadatapb.CLUSTER_IP:    v1.ServiceTypeClusterIP,
	metadatapb.NODE_PORT:     v1.ServiceTypeNodePort,
	metadatapb.LOAD_BALANCER: v1.ServiceTypeLoadBalancer,
	metadatapb.EXTERNAL_NAME: v1.ServiceTypeExternalName,
}

var externalPolicyObjToPbMap = map[v1.ServiceExternalTrafficPolicyType]metadatapb.ExternalTrafficPolicyType{
	v1.ServiceExternalTrafficPolicyTypeLocal:   metadatapb.TRAFFIC_LOCAL,
	v1.ServiceExternalTrafficPolicyTypeCluster: metadatapb.TRAFFIC_CLUSTER,
}

var externalPolicyPbToObjMap = map[metadatapb.ExternalTrafficPolicyType]v1.ServiceExternalTrafficPolicyType{
	metadatapb.TRAFFIC_LOCAL:   v1.ServiceExternalTrafficPolicyTypeLocal,
	metadatapb.TRAFFIC_CLUSTER: v1.ServiceExternalTrafficPolicyTypeCluster,
}

var qosClassObjToPbMap = map[v1.PodQOSClass]metadatapb.PodQOSClass{
	v1.PodQOSGuaranteed: metadatapb.QOS_CLASS_GUARANTEED,
	v1.PodQOSBurstable:  metadatapb.QOS_CLASS_BURSTABLE,
	v1.PodQOSBestEffort: metadatapb.QOS_CLASS_BEST_EFFORT,
}

var qosClassPbToObjMap = map[metadatapb.PodQOSClass]v1.PodQOSClass{
	metadatapb.QOS_CLASS_GUARANTEED:  v1.PodQOSGuaranteed,
	metadatapb.QOS_CLASS_BURSTABLE:   v1.PodQOSBurstable,
	metadatapb.QOS_CLASS_BEST_EFFORT: v1.PodQOSBestEffort,
}

var nodePhaseToPbMap = map[v1.NodePhase]metadatapb.NodePhase{
	v1.NodePending:    metadatapb.NODE_PHASE_PENDING,
	v1.NodeRunning:    metadatapb.NODE_PHASE_RUNNING,
	v1.NodeTerminated: metadatapb.NODE_PHASE_TERMINATED,
}

var nodeAddressTypeToPbMap = map[v1.NodeAddressType]metadatapb.NodeAddressType{
	v1.NodeHostName:    metadatapb.NODE_ADDR_TYPE_HOSTNAME,
	v1.NodeExternalIP:  metadatapb.NODE_ADDR_TYPE_EXTERNAL_IP,
	v1.NodeInternalIP:  metadatapb.NODE_ADDR_TYPE_INTERNAL_IP,
	v1.NodeExternalDNS: metadatapb.NODE_ADDR_TYPE_EXTERNAL_DNS,
	v1.NodeInternalDNS: metadatapb.NODE_ADDR_TYPE_INTERNAL_DNS,
}

var conditionStatusObjToPbMap = map[v1.ConditionStatus]metadatapb.ConditionStatus{
	v1.ConditionTrue:    metadatapb.CONDITION_STATUS_TRUE,
	v1.ConditionFalse:   metadatapb.CONDITION_STATUS_FALSE,
	v1.ConditionUnknown: metadatapb.CONDITION_STATUS_UNKNOWN,
}

var conditionStatusPbToObjMap = map[metadatapb.ConditionStatus]v1.ConditionStatus{
	metadatapb.CONDITION_STATUS_TRUE:    v1.ConditionTrue,
	metadatapb.CONDITION_STATUS_FALSE:   v1.ConditionFalse,
	metadatapb.CONDITION_STATUS_UNKNOWN: v1.ConditionUnknown,
}

var conditionTypeToPbMap = map[v1.NodeConditionType]metadatapb.NodeConditionType{
	v1.NodeReady:              metadatapb.NODE_CONDITION_READY,
	v1.NodeMemoryPressure:     metadatapb.NODE_CONDITION_MEMORY_PRESSURE,
	v1.NodeDiskPressure:       metadatapb.NODE_CONDITION_DISK_PRESSURE,
	v1.NodePIDPressure:        metadatapb.NODE_CONDITION_PID_PRESSURE,
	v1.NodeNetworkUnavailable: metadatapb.NODE_CONDITION_NETWORK_UNAVAILABLE,
}

// OwnerReferenceToProto converts an OwnerReference into a proto.
func OwnerReferenceToProto(o *metav1.OwnerReference) *metadatapb.OwnerReference {
	return &metadatapb.OwnerReference{
		Kind: o.Kind,
		Name: o.Name,
		UID:  string(o.UID),
	}
}

// OwnerReferenceFromProto converts a proto message to an OwnerReference.
func OwnerReferenceFromProto(pb *metadatapb.OwnerReference) *metav1.OwnerReference {
	return &metav1.OwnerReference{
		Kind: pb.Kind,
		Name: pb.Name,
		UID:  types.UID(pb.UID),
	}
}

// ObjectMetadataToProto converts an ObjectMeta into a proto.
func ObjectMetadataToProto(o *metav1.ObjectMeta) *metadatapb.ObjectMetadata {
	ownerRefs := make([]*metadatapb.OwnerReference, len(o.OwnerReferences))

	for i, ref := range o.OwnerReferences {
		ownerRefs[i] = OwnerReferenceToProto(&ref)
	}

	oPb := &metadatapb.ObjectMetadata{
		Name:                o.Name,
		Namespace:           o.Namespace,
		UID:                 string(o.UID),
		ResourceVersion:     o.ResourceVersion,
		OwnerReferences:     ownerRefs,
		Labels:              o.Labels,
		CreationTimestampNS: o.CreationTimestamp.UnixNano(),
		Annotations:         o.Annotations,
	}

	if o.DeletionTimestamp != nil {
		oPb.DeletionTimestampNS = o.DeletionTimestamp.UnixNano()
	}

	return oPb
}

// ObjectMetadataFromProto converts a proto message to an ObjectMeta.
func ObjectMetadataFromProto(pb *metadatapb.ObjectMetadata) *metav1.ObjectMeta {
	var delTime metav1.Time
	var creationTime metav1.Time
	if pb.DeletionTimestampNS != 0 {
		delTime = metav1.Unix(0, pb.DeletionTimestampNS)
	}
	if pb.CreationTimestampNS != 0 {
		creationTime = metav1.Unix(0, pb.CreationTimestampNS)
	}

	ownerRefs := make([]metav1.OwnerReference, len(pb.OwnerReferences))
	for i, refPb := range pb.OwnerReferences {
		ownerRefs[i] = *OwnerReferenceFromProto(refPb)
	}

	return &metav1.ObjectMeta{
		Name:              pb.Name,
		Namespace:         pb.Namespace,
		UID:               types.UID(pb.UID),
		ResourceVersion:   pb.ResourceVersion,
		Labels:            pb.Labels,
		CreationTimestamp: creationTime,
		DeletionTimestamp: &delTime,
		OwnerReferences:   ownerRefs,
	}
}

// PodSpecToProto converts an PodSpec into a proto.
func PodSpecToProto(ps *v1.PodSpec) *metadatapb.PodSpec {
	psPb := &metadatapb.PodSpec{
		NodeSelector:      ps.NodeSelector,
		NodeName:          ps.NodeName,
		Hostname:          ps.Hostname,
		Subdomain:         ps.Subdomain,
		PriorityClassName: ps.PriorityClassName,
		DNSPolicy:         dnsPolicyObjToPbMap[ps.DNSPolicy],
	}
	if ps.Priority != nil {
		psPb.Priority = *ps.Priority
	}

	return psPb
}

// PodSpecFromProto converts a proto message to a PodSpec.
func PodSpecFromProto(pb *metadatapb.PodSpec) *v1.PodSpec {
	return &v1.PodSpec{
		NodeSelector:      pb.NodeSelector,
		NodeName:          pb.NodeName,
		Hostname:          pb.Hostname,
		Subdomain:         pb.Subdomain,
		PriorityClassName: pb.PriorityClassName,
		DNSPolicy:         dnsPolicyPbToObjMap[pb.DNSPolicy],
		Priority:          &pb.Priority,
	}
}

// PodStatusToProto converts an PodStatus into a proto.
func PodStatusToProto(ps *v1.PodStatus) *metadatapb.PodStatus {
	conditions := make([]*metadatapb.PodCondition, len(ps.Conditions))
	for i, c := range ps.Conditions {
		conditions[i] = &metadatapb.PodCondition{
			Type:   podConditionTypeObjToPbMap[c.Type],
			Status: conditionStatusObjToPbMap[c.Status],
		}
	}

	// Following kubectl, take the max # of restarts observed in a container for this pod
	// as the pod's restart count.
	podRestartCount := int64(0)

	containers := make([]*metadatapb.ContainerStatus, len(ps.ContainerStatuses))
	for i, c := range ps.ContainerStatuses {
		containers[i] = ContainerStatusToProto(&c)
		if podRestartCount < int64(c.RestartCount) {
			podRestartCount = int64(c.RestartCount)
		}
	}

	return &metadatapb.PodStatus{
		Message:           ps.Message,
		Reason:            ps.Reason,
		HostIP:            ps.HostIP,
		PodIP:             ps.PodIP,
		Phase:             podPhaseObjToPbMap[ps.Phase],
		Conditions:        conditions,
		QOSClass:          qosClassObjToPbMap[ps.QOSClass],
		ContainerStatuses: containers,
		RestartCount:      podRestartCount,
	}
}

// PodStatusFromProto converts a proto message to a PodStatus.
func PodStatusFromProto(pb *metadatapb.PodStatus) *v1.PodStatus {
	conditions := make([]v1.PodCondition, len(pb.Conditions))
	for i, c := range pb.Conditions {
		conditions[i] = v1.PodCondition{
			Type:   podConditionTypePbToObjMap[c.Type],
			Status: conditionStatusPbToObjMap[c.Status],
		}
	}

	return &v1.PodStatus{
		Message:    pb.Message,
		Reason:     pb.Reason,
		HostIP:     pb.HostIP,
		PodIP:      pb.PodIP,
		Phase:      podPhasePbToObjMap[pb.Phase],
		Conditions: conditions,
		QOSClass:   qosClassPbToObjMap[pb.QOSClass],
	}
}

// PodToProto converts a Pod into a proto.
func PodToProto(p *v1.Pod) *metadatapb.Pod {
	objMeta := ObjectMetadataToProto(&p.ObjectMeta)
	status := PodStatusToProto(&p.Status)

	// If the delete timestamp is set on the pod, then we use a non-K8s pod phase "Terminated" here
	// to indicate the pod has been terminated by the system. If the phase is Failed or Succeeded,
	// we maintain it, since it's clear to the user in that status that the pod isn't running anymore.
	if (status.Phase != metadatapb.FAILED && status.Phase != metadatapb.SUCCEEDED) && objMeta.DeletionTimestampNS != 0 {
		status.Phase = metadatapb.TERMINATED
	}

	return &metadatapb.Pod{
		Metadata: objMeta,
		Spec:     PodSpecToProto(&p.Spec),
		Status:   status,
	}
}

// PodFromProto converts a proto message to a Pod.
func PodFromProto(pb *metadatapb.Pod) *v1.Pod {
	return &v1.Pod{
		ObjectMeta: *ObjectMetadataFromProto(pb.Metadata),
		Spec:       *PodSpecFromProto(pb.Spec),
		Status:     *PodStatusFromProto(pb.Status),
	}
}

// NamespaceToProto converts a namespace into a proto.
func NamespaceToProto(n *v1.Namespace) *metadatapb.Namespace {
	return &metadatapb.Namespace{
		Metadata: ObjectMetadataToProto(&n.ObjectMeta),
	}
}

// ObjectReferenceToProto converts an ObjectReference into a proto.
func ObjectReferenceToProto(o *v1.ObjectReference) *metadatapb.ObjectReference {
	return &metadatapb.ObjectReference{
		Kind:            o.Kind,
		Namespace:       o.Namespace,
		Name:            o.Name,
		UID:             string(o.UID),
		ResourceVersion: o.ResourceVersion,
	}
}

// ObjectReferenceFromProto converts a proto message to a ObjectReference.
func ObjectReferenceFromProto(pb *metadatapb.ObjectReference) *v1.ObjectReference {
	return &v1.ObjectReference{
		Kind:            pb.Kind,
		Namespace:       pb.Namespace,
		Name:            pb.Name,
		UID:             types.UID(pb.UID),
		ResourceVersion: pb.ResourceVersion,
	}
}

// EndpointPortToProto converts an EndpointPort into a proto.
func EndpointPortToProto(e *v1.EndpointPort) *metadatapb.EndpointPort {
	return &metadatapb.EndpointPort{
		Name:     e.Name,
		Port:     e.Port,
		Protocol: ipProtocolObjToPbMap[e.Protocol],
	}
}

// EndpointPortFromProto converts a proto message to a EndpointPort.
func EndpointPortFromProto(pb *metadatapb.EndpointPort) *v1.EndpointPort {
	return &v1.EndpointPort{
		Name:     pb.Name,
		Port:     pb.Port,
		Protocol: ipProtocolPbToObjMap[pb.Protocol],
	}
}

// EndpointAddressToProto converts an EndpointAddress into a proto.
func EndpointAddressToProto(e *v1.EndpointAddress) *metadatapb.EndpointAddress {
	ePb := &metadatapb.EndpointAddress{
		IP:       e.IP,
		Hostname: e.Hostname,
	}
	if e.TargetRef != nil {
		ePb.TargetRef = ObjectReferenceToProto(e.TargetRef)
	}
	if e.NodeName != nil {
		ePb.NodeName = *e.NodeName
	}
	return ePb
}

// EndpointAddressFromProto converts a proto message to a EndpointAddress.
func EndpointAddressFromProto(pb *metadatapb.EndpointAddress) *v1.EndpointAddress {
	e := &v1.EndpointAddress{
		IP:       pb.IP,
		Hostname: pb.Hostname,
		NodeName: &pb.NodeName,
	}

	if pb.TargetRef != nil {
		e.TargetRef = ObjectReferenceFromProto(pb.TargetRef)
	}

	return e
}

// EndpointSubsetToProto converts an EndpointSubset into a proto.
func EndpointSubsetToProto(e *v1.EndpointSubset) *metadatapb.EndpointSubset {
	addresses := make([]*metadatapb.EndpointAddress, len(e.Addresses))
	for i, a := range e.Addresses {
		addresses[i] = EndpointAddressToProto(&a)
	}
	notReadyAddrs := make([]*metadatapb.EndpointAddress, len(e.NotReadyAddresses))
	for i, a := range e.NotReadyAddresses {
		notReadyAddrs[i] = EndpointAddressToProto(&a)
	}
	ports := make([]*metadatapb.EndpointPort, len(e.Ports))
	for i, p := range e.Ports {
		ports[i] = EndpointPortToProto(&p)
	}
	return &metadatapb.EndpointSubset{
		Addresses:         addresses,
		NotReadyAddresses: notReadyAddrs,
		Ports:             ports,
	}
}

// EndpointSubsetFromProto converts a proto message to a EndpointPort.
func EndpointSubsetFromProto(pb *metadatapb.EndpointSubset) *v1.EndpointSubset {
	addresses := make([]v1.EndpointAddress, len(pb.Addresses))
	for i, a := range pb.Addresses {
		addresses[i] = *EndpointAddressFromProto(a)
	}
	notReadyAddrs := make([]v1.EndpointAddress, len(pb.NotReadyAddresses))
	for i, a := range pb.NotReadyAddresses {
		notReadyAddrs[i] = *EndpointAddressFromProto(a)
	}
	ports := make([]v1.EndpointPort, len(pb.Ports))
	for i, p := range pb.Ports {
		ports[i] = *EndpointPortFromProto(p)
	}

	return &v1.EndpointSubset{
		Addresses:         addresses,
		NotReadyAddresses: notReadyAddrs,
		Ports:             ports,
	}
}

// EndpointsToProto converts an Endpoints into a proto.
func EndpointsToProto(e *v1.Endpoints) *metadatapb.Endpoints {
	subsets := make([]*metadatapb.EndpointSubset, len(e.Subsets))
	for i, s := range e.Subsets {
		subsets[i] = EndpointSubsetToProto(&s)
	}

	return &metadatapb.Endpoints{
		Metadata: ObjectMetadataToProto(&e.ObjectMeta),
		Subsets:  subsets,
	}
}

// EndpointsFromProto converts a proto message to an Endpoints.
func EndpointsFromProto(pb *metadatapb.Endpoints) *v1.Endpoints {
	subsets := make([]v1.EndpointSubset, len(pb.Subsets))
	for i, s := range pb.Subsets {
		subsets[i] = *EndpointSubsetFromProto(s)
	}

	return &v1.Endpoints{
		ObjectMeta: *ObjectMetadataFromProto(pb.Metadata),
		Subsets:    subsets,
	}
}

// ServicePortToProto converts a ServicePort into a proto.
func ServicePortToProto(e *v1.ServicePort) *metadatapb.ServicePort {
	return &metadatapb.ServicePort{
		Name:     e.Name,
		Port:     e.Port,
		Protocol: ipProtocolObjToPbMap[e.Protocol],
		NodePort: e.NodePort,
	}
}

// ServicePortFromProto converts a proto message to a ServicePort.
func ServicePortFromProto(pb *metadatapb.ServicePort) *v1.ServicePort {
	return &v1.ServicePort{
		Name:     pb.Name,
		Port:     pb.Port,
		Protocol: ipProtocolPbToObjMap[pb.Protocol],
		NodePort: pb.NodePort,
	}
}

// ServiceSpecToProto converts a ServiceSpec into a proto.
func ServiceSpecToProto(s *v1.ServiceSpec) *metadatapb.ServiceSpec {
	ports := make([]*metadatapb.ServicePort, len(s.Ports))
	for i, p := range s.Ports {
		ports[i] = ServicePortToProto(&p)
	}

	return &metadatapb.ServiceSpec{
		ClusterIP:             s.ClusterIP,
		ExternalIPs:           s.ExternalIPs,
		LoadBalancerIP:        s.LoadBalancerIP,
		ExternalName:          s.ExternalName,
		ExternalTrafficPolicy: externalPolicyObjToPbMap[s.ExternalTrafficPolicy],
		Ports:                 ports,
		Type:                  serviceTypeObjToPbMap[s.Type],
	}
}

// ServiceSpecFromProto converts a proto message to a ServiceSpec.
func ServiceSpecFromProto(pb *metadatapb.ServiceSpec) *v1.ServiceSpec {
	ports := make([]v1.ServicePort, len(pb.Ports))
	for i, p := range pb.Ports {
		ports[i] = *ServicePortFromProto(p)
	}

	return &v1.ServiceSpec{
		ClusterIP:             pb.ClusterIP,
		ExternalIPs:           pb.ExternalIPs,
		LoadBalancerIP:        pb.LoadBalancerIP,
		ExternalName:          pb.ExternalName,
		ExternalTrafficPolicy: externalPolicyPbToObjMap[pb.ExternalTrafficPolicy],
		Type:                  serviceTypePbToObjMap[pb.Type],
		Ports:                 ports,
	}
}

// ServiceToProto converts a Service into a proto.
func ServiceToProto(s *v1.Service) *metadatapb.Service {
	return &metadatapb.Service{
		Metadata: ObjectMetadataToProto(&s.ObjectMeta),
		Spec:     ServiceSpecToProto(&s.Spec),
	}
}

// ServiceFromProto converts a proto message to a Service.
func ServiceFromProto(pb *metadatapb.Service) *v1.Service {
	return &v1.Service{
		ObjectMeta: *ObjectMetadataFromProto(pb.Metadata),
		Spec:       *ServiceSpecFromProto(pb.Spec),
	}
}

// ContainerStatusToProto converts a ContainerStatus into a proto.
func ContainerStatusToProto(c *v1.ContainerStatus) *metadatapb.ContainerStatus {
	cPb := &metadatapb.ContainerStatus{
		Name:         c.Name,
		ContainerID:  c.ContainerID,
		RestartCount: int64(c.RestartCount),
	}
	switch {
	case c.State.Waiting != nil:
		cPb.ContainerState = metadatapb.CONTAINER_STATE_WAITING
		cPb.Message = c.State.Waiting.Message
		cPb.Reason = c.State.Waiting.Reason
	case c.State.Running != nil:
		cPb.ContainerState = metadatapb.CONTAINER_STATE_RUNNING
		cPb.StartTimestampNS = c.State.Running.StartedAt.UnixNano()
	case c.State.Terminated != nil:
		cPb.ContainerState = metadatapb.CONTAINER_STATE_TERMINATED
		cPb.StartTimestampNS = c.State.Terminated.StartedAt.UnixNano()
		cPb.StopTimestampNS = c.State.Terminated.FinishedAt.UnixNano()
		cPb.Message = c.State.Terminated.Message
		cPb.Reason = c.State.Terminated.Reason
	}
	return cPb
}

// NodeToProto converts a k8s Node object into a proto.
func NodeToProto(n *v1.Node) *metadatapb.Node {
	return &metadatapb.Node{
		Metadata: ObjectMetadataToProto(&n.ObjectMeta),
		Spec:     NodeSpecToProto(&n.Spec),
		Status:   NodeStatusToProto(&n.Status),
	}
}

// NodeStatusToProto converts a k8s Node status into a proto.
func NodeStatusToProto(n *v1.NodeStatus) *metadatapb.NodeStatus {
	addrs := make([]*metadatapb.NodeAddress, len(n.Addresses))
	for i, a := range n.Addresses {
		addrs[i] = &metadatapb.NodeAddress{
			Type:    nodeAddressTypeToPbMap[a.Type],
			Address: a.Address,
		}
	}

	conds := make([]*metadatapb.NodeCondition, len(n.Conditions))
	for i, c := range n.Conditions {
		conds[i] = &metadatapb.NodeCondition{
			Type:   conditionTypeToPbMap[c.Type],
			Status: conditionStatusObjToPbMap[c.Status],
		}
	}

	return &metadatapb.NodeStatus{
		Phase:      nodePhaseToPbMap[n.Phase],
		Addresses:  addrs,
		Conditions: conds,
	}
}

// NodeSpecToProto converts a k8s Node spec into a proto.
func NodeSpecToProto(n *v1.NodeSpec) *metadatapb.NodeSpec {
	return &metadatapb.NodeSpec{
		PodCIDRs: n.PodCIDRs,
		PodCIDR:  n.PodCIDR,
	}
}

// ReplicaSetToProto converts a k8s ReplicaSet object into a proto.
func ReplicaSetToProto(rs *apps.ReplicaSet) *metadatapb.ReplicaSet {
	return &metadatapb.ReplicaSet{
		Metadata: ObjectMetadataToProto(&rs.ObjectMeta),
		Spec:     ReplicaSetSpecToProto(&rs.Spec),
		Status:   ReplicaSetStatusToProto(&rs.Status),
	}
}

// ReplicaSetStatusToProto converts a k8s ReplicaSet status into a proto.
func ReplicaSetStatusToProto(rss *apps.ReplicaSetStatus) *metadatapb.ReplicaSetStatus {
	conditions := make([]*metadatapb.ReplicaSetCondition, len(rss.Conditions))
	for i, c := range rss.Conditions {
		conditions[i] = &metadatapb.ReplicaSetCondition{
			Type:   string(c.Type),
			Status: conditionStatusObjToPbMap[c.Status],
		}
	}

	return &metadatapb.ReplicaSetStatus{
		Replicas:             rss.Replicas,
		FullyLabeledReplicas: rss.FullyLabeledReplicas,
		ReadyReplicas:        rss.ReadyReplicas,
		AvailableReplicas:    rss.AvailableReplicas,
		ObservedGeneration:   rss.ObservedGeneration,
		Conditions:           conditions,
	}
}

// ReplicaSetSpecToProto converts a k8s ReplicaSet spec into a proto.
func ReplicaSetSpecToProto(rs *apps.ReplicaSetSpec) *metadatapb.ReplicaSetSpec {
	var replicas int32
	if rs.Replicas == nil {
		replicas = 1
	} else {
		replicas = int32(*rs.Replicas)
	}

	return &metadatapb.ReplicaSetSpec{
		Replicas:        replicas,
		MinReadySeconds: rs.MinReadySeconds,
		Selector:        LabelSelectorToProto(rs.Selector),
		Template:        PodTemplateSpecToProto(rs.Template),
	}
}

// LabelSelectorToProto converts a k8s label selector to proto.
func LabelSelectorToProto(ls *metav1.LabelSelector) *metadatapb.LabelSelector {
	matchExpressions := make([]*metadatapb.LabelSelectorRequirement, len(ls.MatchExpressions))

	for i, me := range ls.MatchExpressions {
		matchExpressions[i] = &metadatapb.LabelSelectorRequirement{
			Key:      me.Key,
			Operator: string(me.Operator),
			Values:   me.Values,
		}
	}

	return &metadatapb.LabelSelector{
		MatchLabels:      ls.MatchLabels,
		MatchExpressions: matchExpressions,
	}
}

// PodTemplateSpecToProto converts a k8s pod template spec to proto.
func PodTemplateSpecToProto(ts v1.PodTemplateSpec) *metadatapb.PodTemplateSpec {
	return &metadatapb.PodTemplateSpec{
		Metadata: ObjectMetadataToProto(&ts.ObjectMeta),
		Spec:     PodSpecToProto(&ts.Spec),
	}
}

// IntOrStringToString converts intstr object to string
func IntOrStringToString(v *intstr.IntOrString) string {
	switch v.Type {
	case 0:
		return fmt.Sprint(v.IntVal)
	default:
		return v.StrVal
	}
}

// DeploymentSpecToProto converts apps deployment spec to proto
func DeploymentSpecToProto(d *apps.DeploymentSpec) *metadatapb.DeploymentSpec {
	var sType metadatapb.DeploymentStrategyType
	switch d.Strategy.Type {
	case "Recreate":
		sType = metadatapb.DEPLOYMENT_STRATEGY_RECREATE
	case "RollingUpdate":
		sType = metadatapb.DEPLOYMENT_STRATEGY_ROLLING_UPDATE
	default:
		sType = metadatapb.DEPLOYMENT_STRATEGY_UNKNOWN
	}

	var rollingUpdate *metadatapb.RollingUpdateDeployment
	if d.Strategy.Type == "RollingUpdate" {
		rollingUpdate = &metadatapb.RollingUpdateDeployment{
			MaxUnavailable: IntOrStringToString(d.Strategy.RollingUpdate.MaxUnavailable),
			MaxSurge:       IntOrStringToString(d.Strategy.RollingUpdate.MaxSurge),
		}
	}

	var replicas, revisionHistoryLimit, progressDeadlineSeconds int32
	if d.Replicas != nil {
		replicas = *d.Replicas
	}
	if d.RevisionHistoryLimit != nil {
		revisionHistoryLimit = *d.RevisionHistoryLimit
	}
	if d.ProgressDeadlineSeconds != nil {
		progressDeadlineSeconds = *d.ProgressDeadlineSeconds
	}

	return &metadatapb.DeploymentSpec{
		Replicas: replicas,
		Selector: LabelSelectorToProto(d.Selector),
		Template: PodTemplateSpecToProto(d.Template),
		Strategy: &metadatapb.DeploymentStrategy{
			Type:          sType,
			RollingUpdate: rollingUpdate,
		},
		MinReadySeconds:         d.MinReadySeconds,
		RevisionHistoryLimit:    revisionHistoryLimit,
		Paused:                  d.Paused,
		ProgressDeadlineSeconds: progressDeadlineSeconds,
	}
}

// DeploymentConditionToProto converts apps DeploymentCondition to proto
func DeploymentConditionToProto(d *apps.DeploymentCondition) *metadatapb.DeploymentCondition {
	var dType metadatapb.DeploymentConditionType
	switch d.Type {
	case "Available":
		dType = metadatapb.DEPLOYMENT_CONDITION_AVAILABLE
	case "Progressing":
		dType = metadatapb.DEPLOYMENT_CONDITION_PROGRESSING
	case "ReplicaFailure":
		dType = metadatapb.DEPLOYMENT_CONDITION_REPLICA_FAILURE
	default:
		dType = metadatapb.DEPLOYMENT_CONDITION_TYPE_UNKNOWN
	}

	return &metadatapb.DeploymentCondition{
		Type:                 dType,
		Status:               conditionStatusObjToPbMap[d.Status],
		LastUpdateTimeNS:     d.LastUpdateTime.UnixNano(),
		LastTransitionTimeNS: d.LastTransitionTime.UnixNano(),
		Reason:               d.Reason,
		Message:              d.Message,
	}
}

// DeploymentStatusToProto converts apps.DeploymentStatus to proto
func DeploymentStatusToProto(d *apps.DeploymentStatus) *metadatapb.DeploymentStatus {
	var conditions []*metadatapb.DeploymentCondition
	for _, c := range d.Conditions {
		conditions = append(conditions, DeploymentConditionToProto(&c))
	}

	var collisionCount int32
	if d.CollisionCount != nil {
		collisionCount = *d.CollisionCount
	}

	return &metadatapb.DeploymentStatus{
		ObservedGeneration:  d.ObservedGeneration,
		Replicas:            d.Replicas,
		UpdatedReplicas:     d.UpdatedReplicas,
		ReadyReplicas:       d.ReadyReplicas,
		AvailableReplicas:   d.AvailableReplicas,
		UnavailableReplicas: d.UnavailableReplicas,
		Conditions:          conditions,
		CollisionCount:      collisionCount,
	}
}

// DeploymentToProto converts apps.Deployment to proto
func DeploymentToProto(d *apps.Deployment) *metadatapb.Deployment {
	return &metadatapb.Deployment{
		Metadata: ObjectMetadataToProto(&d.ObjectMeta),
		Spec:     DeploymentSpecToProto(&d.Spec),
		Status:   DeploymentStatusToProto(&d.Status),
	}
}
