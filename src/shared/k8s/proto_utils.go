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
	v1.PodFailed:    metadatapb.FAILED,
	v1.PodUnknown:   metadatapb.PHASE_UNKNOWN,
}

var podPhasePbToObjMap = map[metadatapb.PodPhase]v1.PodPhase{
	metadatapb.PENDING:       v1.PodPending,
	metadatapb.RUNNING:       v1.PodRunning,
	metadatapb.SUCCEEDED:     v1.PodSucceeded,
	metadatapb.FAILED:        v1.PodFailed,
	metadatapb.PHASE_UNKNOWN: v1.PodUnknown,
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

var podConditionStatusObjToPbMap = map[v1.ConditionStatus]metadatapb.PodConditionStatus{
	v1.ConditionTrue:    metadatapb.STATUS_TRUE,
	v1.ConditionFalse:   metadatapb.STATUS_FALSE,
	v1.ConditionUnknown: metadatapb.STATUS_UNKNOWN,
}

var podConditionStatusPbToObjMap = map[metadatapb.PodConditionStatus]v1.ConditionStatus{
	metadatapb.STATUS_TRUE:    v1.ConditionTrue,
	metadatapb.STATUS_FALSE:   v1.ConditionFalse,
	metadatapb.STATUS_UNKNOWN: v1.ConditionUnknown,
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

// OwnerReferenceToProto converts an OwnerReference into a proto.
func OwnerReferenceToProto(o *metav1.OwnerReference) (*metadatapb.OwnerReference, error) {
	oPb := &metadatapb.OwnerReference{
		Kind: o.Kind,
		Name: o.Name,
		UID:  string(o.UID),
	}
	return oPb, nil
}

// OwnerReferenceFromProto converts a proto message to an OwnerReference.
func OwnerReferenceFromProto(pb *metadatapb.OwnerReference) (*metav1.OwnerReference, error) {
	obj := &metav1.OwnerReference{
		Kind: pb.Kind,
		Name: pb.Name,
		UID:  types.UID(pb.UID),
	}
	return obj, nil
}

// ObjectMetadataToProto converts an ObjectMeta into a proto.
func ObjectMetadataToProto(o *metav1.ObjectMeta) (*metadatapb.ObjectMetadata, error) {
	ownerRefs := make([]*metadatapb.OwnerReference, len(o.OwnerReferences))

	for i, ref := range o.OwnerReferences {
		refPb, err := OwnerReferenceToProto(&ref)
		if err != nil {
			return nil, err
		}
		ownerRefs[i] = refPb
	}

	oPb := &metadatapb.ObjectMetadata{
		Name:                o.Name,
		Namespace:           o.Namespace,
		UID:                 string(o.UID),
		ResourceVersion:     o.ResourceVersion,
		ClusterName:         o.ClusterName,
		OwnerReferences:     ownerRefs,
		Labels:              o.Labels,
		CreationTimestampNS: o.CreationTimestamp.UnixNano(),
	}

	if o.DeletionTimestamp != nil {
		oPb.DeletionTimestampNS = o.DeletionTimestamp.UnixNano()
	}

	return oPb, nil
}

// ObjectMetadataFromProto converts a proto message to an ObjectMeta.
func ObjectMetadataFromProto(pb *metadatapb.ObjectMetadata) (*metav1.ObjectMeta, error) {
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
		ref, err := OwnerReferenceFromProto(refPb)
		if err != nil {
			return &metav1.ObjectMeta{}, nil
		}
		ownerRefs[i] = *ref
	}

	o := &metav1.ObjectMeta{
		Name:              pb.Name,
		Namespace:         pb.Namespace,
		UID:               types.UID(pb.UID),
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
		DNSPolicy:         dnsPolicyObjToPbMap[ps.DNSPolicy],
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
		DNSPolicy:         dnsPolicyPbToObjMap[pb.DNSPolicy],
		Priority:          &pb.Priority,
	}

	return ps, nil
}

// PodStatusToProto converts an PodStatus into a proto.
func PodStatusToProto(ps *v1.PodStatus) (*metadatapb.PodStatus, error) {
	conditions := make([]*metadatapb.PodCondition, len(ps.Conditions))
	for i, c := range ps.Conditions {
		conditions[i] = &metadatapb.PodCondition{
			Type:   podConditionTypeObjToPbMap[c.Type],
			Status: podConditionStatusObjToPbMap[c.Status],
		}
	}

	containers := make([]*metadatapb.ContainerStatus, len(ps.ContainerStatuses))
	for i, c := range ps.ContainerStatuses {
		cPb, err := ContainerStatusToProto(&c)
		if err != nil {
			return &metadatapb.PodStatus{}, err
		}
		containers[i] = cPb
	}

	psPb := &metadatapb.PodStatus{
		Message:           ps.Message,
		Reason:            ps.Reason,
		HostIP:            ps.HostIP,
		PodIP:             ps.PodIP,
		Phase:             podPhaseObjToPbMap[ps.Phase],
		Conditions:        conditions,
		QOSClass:          qosClassObjToPbMap[ps.QOSClass],
		ContainerStatuses: containers,
	}

	return psPb, nil
}

// PodStatusFromProto converts a proto message to a PodStatus.
func PodStatusFromProto(pb *metadatapb.PodStatus) (*v1.PodStatus, error) {
	conditions := make([]v1.PodCondition, len(pb.Conditions))
	for i, c := range pb.Conditions {
		podCond := v1.PodCondition{
			Type:   podConditionTypePbToObjMap[c.Type],
			Status: podConditionStatusPbToObjMap[c.Status],
		}
		conditions[i] = podCond
	}

	ps := &v1.PodStatus{
		Message:    pb.Message,
		Reason:     pb.Reason,
		HostIP:     pb.HostIP,
		PodIP:      pb.PodIP,
		Phase:      podPhasePbToObjMap[pb.Phase],
		Conditions: conditions,
		QOSClass:   qosClassPbToObjMap[pb.QOSClass],
	}

	return ps, nil
}

// PodToProto converts a Pod into a proto.
func PodToProto(p *v1.Pod) (*metadatapb.Pod, error) {
	metadata, err := ObjectMetadataToProto(&p.ObjectMeta)
	if err != nil {
		return nil, err
	}

	spec, err := PodSpecToProto(&p.Spec)
	if err != nil {
		return nil, err
	}

	status, err := PodStatusToProto(&p.Status)
	if err != nil {
		return nil, err
	}

	pPb := &metadatapb.Pod{
		Metadata: metadata,
		Spec:     spec,
		Status:   status,
	}
	return pPb, nil
}

// PodFromProto converts a proto message to a Pod.
func PodFromProto(pb *metadatapb.Pod) (*v1.Pod, error) {
	metadata, err := ObjectMetadataFromProto(pb.Metadata)
	if err != nil {
		return nil, err
	}

	spec, err := PodSpecFromProto(pb.Spec)
	if err != nil {
		return nil, err
	}

	status, err := PodStatusFromProto(pb.Status)
	if err != nil {
		return nil, err
	}

	p := &v1.Pod{
		ObjectMeta: *metadata,
		Spec:       *spec,
		Status:     *status,
	}

	return p, nil
}

// NamespaceToProto converts a namespace into a proto.
func NamespaceToProto(n *v1.Namespace) (*metadatapb.Namespace, error) {
	metadata, err := ObjectMetadataToProto(&n.ObjectMeta)
	if err != nil {
		return nil, err
	}

	nPb := &metadatapb.Namespace{
		Metadata: metadata,
	}
	return nPb, nil
}

// ObjectReferenceToProto converts an ObjectReference into a proto.
func ObjectReferenceToProto(o *v1.ObjectReference) (*metadatapb.ObjectReference, error) {
	oPb := &metadatapb.ObjectReference{
		Kind:            o.Kind,
		Namespace:       o.Namespace,
		Name:            o.Name,
		UID:             string(o.UID),
		ResourceVersion: o.ResourceVersion,
	}
	return oPb, nil
}

// ObjectReferenceFromProto converts a proto message to a ObjectReference.
func ObjectReferenceFromProto(pb *metadatapb.ObjectReference) (*v1.ObjectReference, error) {
	o := &v1.ObjectReference{
		Kind:            pb.Kind,
		Namespace:       pb.Namespace,
		Name:            pb.Name,
		UID:             types.UID(pb.UID),
		ResourceVersion: pb.ResourceVersion,
	}

	return o, nil
}

// EndpointPortToProto converts an EndpointPort into a proto.
func EndpointPortToProto(e *v1.EndpointPort) (*metadatapb.EndpointPort, error) {
	ePb := &metadatapb.EndpointPort{
		Name:     e.Name,
		Port:     e.Port,
		Protocol: ipProtocolObjToPbMap[e.Protocol],
	}
	return ePb, nil
}

// EndpointPortFromProto converts a proto message to a EndpointPort.
func EndpointPortFromProto(pb *metadatapb.EndpointPort) (*v1.EndpointPort, error) {
	e := &v1.EndpointPort{
		Name:     pb.Name,
		Port:     pb.Port,
		Protocol: ipProtocolPbToObjMap[pb.Protocol],
	}

	return e, nil
}

// EndpointAddressToProto converts an EndpointAddress into a proto.
func EndpointAddressToProto(e *v1.EndpointAddress) (*metadatapb.EndpointAddress, error) {
	nodename := ""
	if e.NodeName != nil {
		nodename = *e.NodeName
	}

	ePb := &metadatapb.EndpointAddress{
		IP:       e.IP,
		Hostname: e.Hostname,
		NodeName: nodename,
	}
	if e.TargetRef != nil {
		objRef, err := ObjectReferenceToProto(e.TargetRef)
		if err != nil {
			return nil, err
		}
		ePb.TargetRef = objRef
	}
	return ePb, nil
}

// EndpointAddressFromProto converts a proto message to a EndpointAddress.
func EndpointAddressFromProto(pb *metadatapb.EndpointAddress) (*v1.EndpointAddress, error) {
	e := &v1.EndpointAddress{
		IP:       pb.IP,
		Hostname: pb.Hostname,
		NodeName: &pb.NodeName,
	}

	if pb.TargetRef != nil {
		objRef, err := ObjectReferenceFromProto(pb.TargetRef)
		if err != nil {
			return nil, err
		}
		e.TargetRef = objRef
	}

	return e, nil
}

// EndpointSubsetToProto converts an EndpointSubset into a proto.
func EndpointSubsetToProto(e *v1.EndpointSubset) (*metadatapb.EndpointSubset, error) {
	addresses := make([]*metadatapb.EndpointAddress, len(e.Addresses))
	for i, a := range e.Addresses {
		aPb, err := EndpointAddressToProto(&a)
		if err != nil {
			return nil, err
		}
		addresses[i] = aPb
	}
	notReadyAddrs := make([]*metadatapb.EndpointAddress, len(e.NotReadyAddresses))
	for i, a := range e.NotReadyAddresses {
		aPb, err := EndpointAddressToProto(&a)
		if err != nil {
			return nil, err
		}
		notReadyAddrs[i] = aPb
	}
	ports := make([]*metadatapb.EndpointPort, len(e.Ports))
	for i, p := range e.Ports {
		pPb, err := EndpointPortToProto(&p)
		if err != nil {
			return nil, err
		}
		ports[i] = pPb
	}
	ePb := &metadatapb.EndpointSubset{
		Addresses:         addresses,
		NotReadyAddresses: notReadyAddrs,
		Ports:             ports,
	}
	return ePb, nil
}

// EndpointSubsetFromProto converts a proto message to a EndpointPort.
func EndpointSubsetFromProto(pb *metadatapb.EndpointSubset) (*v1.EndpointSubset, error) {
	addresses := make([]v1.EndpointAddress, len(pb.Addresses))
	for i, a := range pb.Addresses {
		aPb, err := EndpointAddressFromProto(a)
		if err != nil {
			return nil, err
		}
		addresses[i] = *aPb
	}
	notReadyAddrs := make([]v1.EndpointAddress, len(pb.NotReadyAddresses))
	for i, a := range pb.NotReadyAddresses {
		aPb, err := EndpointAddressFromProto(a)
		if err != nil {
			return nil, err
		}
		notReadyAddrs[i] = *aPb
	}
	ports := make([]v1.EndpointPort, len(pb.Ports))
	for i, p := range pb.Ports {
		pPb, err := EndpointPortFromProto(p)
		if err != nil {
			return nil, err
		}
		ports[i] = *pPb
	}

	e := &v1.EndpointSubset{
		Addresses:         addresses,
		NotReadyAddresses: notReadyAddrs,
		Ports:             ports,
	}

	return e, nil
}

// EndpointsToProto converts an Endpoints into a proto.
func EndpointsToProto(e *v1.Endpoints) (*metadatapb.Endpoints, error) {
	metadata, err := ObjectMetadataToProto(&e.ObjectMeta)
	if err != nil {
		return nil, err
	}

	subsets := make([]*metadatapb.EndpointSubset, len(e.Subsets))
	for i, s := range e.Subsets {
		sPb, err := EndpointSubsetToProto(&s)
		if err != nil {
			return nil, err
		}
		subsets[i] = sPb
	}

	ePb := &metadatapb.Endpoints{
		Metadata: metadata,
		Subsets:  subsets,
	}

	return ePb, nil
}

// EndpointsFromProto converts a proto message to an Endpoints.
func EndpointsFromProto(pb *metadatapb.Endpoints) (*v1.Endpoints, error) {
	md, err := ObjectMetadataFromProto(pb.Metadata)
	if err != nil {
		return nil, err
	}

	subsets := make([]v1.EndpointSubset, len(pb.Subsets))
	for i, s := range pb.Subsets {
		sPb, err := EndpointSubsetFromProto(s)
		if err != nil {
			return nil, err
		}
		subsets[i] = *sPb
	}

	e := &v1.Endpoints{
		ObjectMeta: *md,
		Subsets:    subsets,
	}

	return e, nil
}

// ServicePortToProto converts a ServicePort into a proto.
func ServicePortToProto(e *v1.ServicePort) (*metadatapb.ServicePort, error) {
	sPb := &metadatapb.ServicePort{
		Name:     e.Name,
		Port:     e.Port,
		Protocol: ipProtocolObjToPbMap[e.Protocol],
		NodePort: e.NodePort,
	}
	return sPb, nil
}

// ServicePortFromProto converts a proto message to a ServicePort.
func ServicePortFromProto(pb *metadatapb.ServicePort) (*v1.ServicePort, error) {
	s := &v1.ServicePort{
		Name:     pb.Name,
		Port:     pb.Port,
		Protocol: ipProtocolPbToObjMap[pb.Protocol],
		NodePort: pb.NodePort,
	}

	return s, nil
}

// ServiceSpecToProto converts a ServiceSpec into a proto.
func ServiceSpecToProto(s *v1.ServiceSpec) (*metadatapb.ServiceSpec, error) {
	ports := make([]*metadatapb.ServicePort, len(s.Ports))
	for i, p := range s.Ports {
		pPb, err := ServicePortToProto(&p)
		if err != nil {
			return nil, err
		}
		ports[i] = pPb
	}

	sPb := &metadatapb.ServiceSpec{
		ClusterIP:             s.ClusterIP,
		ExternalIPs:           s.ExternalIPs,
		LoadBalancerIP:        s.LoadBalancerIP,
		ExternalName:          s.ExternalName,
		ExternalTrafficPolicy: externalPolicyObjToPbMap[s.ExternalTrafficPolicy],
		Ports:                 ports,
		Type:                  serviceTypeObjToPbMap[s.Type],
	}

	return sPb, nil
}

// ServiceSpecFromProto converts a proto message to a ServiceSpec.
func ServiceSpecFromProto(pb *metadatapb.ServiceSpec) (*v1.ServiceSpec, error) {
	ports := make([]v1.ServicePort, len(pb.Ports))
	for i, p := range pb.Ports {
		pPb, err := ServicePortFromProto(p)
		if err != nil {
			return nil, err
		}
		ports[i] = *pPb
	}

	s := &v1.ServiceSpec{
		ClusterIP:             pb.ClusterIP,
		ExternalIPs:           pb.ExternalIPs,
		LoadBalancerIP:        pb.LoadBalancerIP,
		ExternalName:          pb.ExternalName,
		ExternalTrafficPolicy: externalPolicyPbToObjMap[pb.ExternalTrafficPolicy],
		Type:                  serviceTypePbToObjMap[pb.Type],
		Ports:                 ports,
	}

	return s, nil
}

// ServiceToProto converts a Service into a proto.
func ServiceToProto(s *v1.Service) (*metadatapb.Service, error) {
	metadata, err := ObjectMetadataToProto(&s.ObjectMeta)
	if err != nil {
		return nil, err
	}

	spec, err := ServiceSpecToProto(&s.Spec)
	if err != nil {
		return nil, err
	}

	sPb := &metadatapb.Service{
		Metadata: metadata,
		Spec:     spec,
	}
	return sPb, nil
}

// ServiceFromProto converts a proto message to a Service.
func ServiceFromProto(pb *metadatapb.Service) (*v1.Service, error) {
	metadata, err := ObjectMetadataFromProto(pb.Metadata)
	if err != nil {
		return nil, err
	}

	spec, err := ServiceSpecFromProto(pb.Spec)
	if err != nil {
		return nil, err
	}

	s := &v1.Service{
		ObjectMeta: *metadata,
		Spec:       *spec,
	}

	return s, nil
}

// ContainerStatusToProto converts a ContainerStatus into a proto.
func ContainerStatusToProto(c *v1.ContainerStatus) (*metadatapb.ContainerStatus, error) {
	cPb := &metadatapb.ContainerStatus{
		Name:        c.Name,
		ContainerID: c.ContainerID,
	}
	if c.State.Waiting != nil {
		cPb.ContainerState = metadatapb.CONTAINER_STATE_WAITING
		cPb.Message = c.State.Waiting.Message
		cPb.Reason = c.State.Waiting.Reason
	} else if c.State.Running != nil {
		cPb.ContainerState = metadatapb.CONTAINER_STATE_RUNNING
		cPb.StartTimestampNS = c.State.Running.StartedAt.UnixNano()
	} else if c.State.Terminated != nil {
		cPb.ContainerState = metadatapb.CONTAINER_STATE_TERMINATED
		cPb.StartTimestampNS = c.State.Terminated.StartedAt.UnixNano()
		cPb.StopTimestampNS = c.State.Terminated.FinishedAt.UnixNano()
		cPb.Message = c.State.Terminated.Message
		cPb.Reason = c.State.Terminated.Reason
	}

	return cPb, nil
}

// NodeToProto converts a k8s Node object into a proto.
func NodeToProto(n *v1.Node) (*metadatapb.Node, error) {
	metadata, err := ObjectMetadataToProto(&n.ObjectMeta)
	if err != nil {
		return nil, err
	}

	spec := NodeSpecToProto(&n.Spec)

	status := NodeStatusToProto(&n.Status)

	return &metadatapb.Node{
		Metadata: metadata,
		Spec:     spec,
		Status:   status,
	}, nil
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

	return &metadatapb.NodeStatus{
		Phase:     nodePhaseToPbMap[n.Phase],
		Addresses: addrs,
	}
}

// NodeSpecToProto converts a k8s Node spec into a proto.
func NodeSpecToProto(n *v1.NodeSpec) *metadatapb.NodeSpec {
	return &metadatapb.NodeSpec{
		PodCIDRs: n.PodCIDRs,
		PodCIDR:  n.PodCIDR,
	}
}
