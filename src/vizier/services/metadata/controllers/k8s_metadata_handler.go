package controllers

import (
	"fmt"
	"net"
	"reflect"
	"strings"
	"sync"

	"github.com/EvilSuperstars/go-cidrman"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	k8stypes "k8s.io/apimachinery/pkg/types"
	"k8s.io/apimachinery/pkg/watch"

	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

// KelvinUpdateChannel is the channel that all kelvins updates are sent on.
const KelvinUpdateChannel = "all"

// K8sMessage is a message for K8s metadata events/updates.
type K8sMessage struct {
	Object     runtime.Object
	ObjectType string
	EventType  watch.EventType
}

// OutgoingUpdate is an outgoing resource message that should be sent to NATS on the provided channels.
type OutgoingUpdate struct {
	// Update is the ResourceUpdate that should be sent out.
	Update *metadatapb.ResourceUpdate
	// Topics are the channels that the update should be sent to. These are usually IPs, to be sent to specific
	// agents.
	Topics []string
}

// K8sMetadataStore handles storing and fetching any data related to K8s resources.
type K8sMetadataStore interface {
	// AddResourceUpdates stores the given resource with its associated resourceVersion for 24h.
	AddResourceUpdate(resourceVersion string, resource *storepb.K8SResource) error
	// FetchResourceUpdates gets the resource updates from the `from` resource version, to the `to`
	// resource version (exclusive).
	FetchResourceUpdates(from string, to string) ([]*storepb.K8SResource, error)
}

// An UpdateProcessor is responsible for processing an incoming update, such as determining what
// updates should be persisted and sent to NATS.
type UpdateProcessor interface {
	// SetDeleted sets the deletion timestamp for the object, if there is none already set.
	SetDeleted(runtime.Object)
	// ValidateUpdate checks whether the update is valid and should be further processed.
	ValidateUpdate(runtime.Object, *ProcessorState) bool
	// GetStoredProtos gets the protos that should be persisted in the data store, derived from
	// the given update.
	GetStoredProtos(runtime.Object) (updates []*storepb.K8SResource, resourceVersions []string)
	// GetUpdatesToSend gets all of the updates that should be sent to the agents, along with the relevant IPs that
	// the updates should be sent to. If the IP is empty, this means the update should be sent to all known IPs, including
	// kelvin.
	GetUpdatesToSend(runtime.Object, *ProcessorState) []*OutgoingUpdate
}

// ProcessorState is data that should be shared across update processors. It can contain that needs
// to be cached across all updates, such as CIDRs or tracking leader election messages.
type ProcessorState struct {
	// This is a cache of all the leader election message. This is used to avoid sending excessive
	// endpoints updates.
	LeaderMsgs  map[k8stypes.UID]*v1.Endpoints
	ServiceCIDR *net.IPNet // This is the service CIDR block; it is inferred from all observed service IPs.
	PodCIDRs    []string   // The pod CIDRs in the cluster, inferred from each node's reported pod CIDR.
	// A map from node name to its internal IP.
	NodeToIP map[string]string
	// A map from pod name to its IP.
	PodToIP map[string]string
}

// K8sMetadataHandler handles any incoming k8s updates. It saves the update to the store for persistence, and
// also sends the update to the relevant IP channel.
type K8sMetadataHandler struct {
	// The channel in which incoming k8s updates are sent to.
	updateCh <-chan *K8sMessage
	// The store where k8s resources are stored.
	mds K8sMetadataStore
	// The NATS connection on which to send messages on.
	conn *nats.Conn
	// Done channel, to stop processing metadata updates.
	done chan struct{}
	// A map from object type to the update processor that should handle that type.
	processHandlerMap map[string]UpdateProcessor

	// State that should be shared across all update processors.
	state ProcessorState
	once  sync.Once
}

// NewK8sMetadataHandler creates a new K8sMetadataHandler.
func NewK8sMetadataHandler(updateCh <-chan *K8sMessage, mds K8sMetadataStore, conn *nats.Conn) *K8sMetadataHandler {
	done := make(chan struct{})
	leaderMsgs := make(map[k8stypes.UID]*v1.Endpoints)
	handlerMap := make(map[string]UpdateProcessor)
	state := ProcessorState{LeaderMsgs: leaderMsgs, PodCIDRs: make([]string, 0), NodeToIP: make(map[string]string), PodToIP: make(map[string]string)}
	mh := &K8sMetadataHandler{updateCh: updateCh, mds: mds, conn: conn, done: done, processHandlerMap: handlerMap, state: state}

	// Register update processors.
	mh.processHandlerMap["endpoints"] = &EndpointsUpdateProcessor{}
	mh.processHandlerMap["services"] = &ServiceUpdateProcessor{}
	mh.processHandlerMap["pods"] = &PodUpdateProcessor{}
	mh.processHandlerMap["nodes"] = &NodeUpdateProcessor{}
	mh.processHandlerMap["namespaces"] = &NamespaceUpdateProcessor{}

	go mh.processUpdates()
	return mh
}

func (m *K8sMetadataHandler) processUpdates() {
	for {
		select {
		case <-m.done:
			return
		case msg := <-m.updateCh:
			var processor UpdateProcessor

			p, ok := m.processHandlerMap[msg.ObjectType]
			if !ok {
				log.Error("Received unknown metadata message with type: " + msg.ObjectType)
				continue
			}
			processor = p

			update := msg.Object
			if msg.EventType == watch.Deleted {
				processor.SetDeleted(update)
			}

			// Check that the update is valid and should be handled.
			valid := processor.ValidateUpdate(update, &m.state)
			if !valid {
				continue
			}
			// Persist the update in the data store.
			updates, rvs := processor.GetStoredProtos(update)
			if updates == nil {
				continue
			}
			for i, rv := range rvs {
				err := m.mds.AddResourceUpdate(rv, updates[i])
				if err != nil {
					log.WithError(err).Error("Failed to store resource update")
					continue
				}
			}
		}
	}
}

func setDeleted(objMeta *metav1.ObjectMeta) {
	if objMeta.DeletionTimestamp != nil {
		// Deletion timestamp already set.
		return
	}
	now := metav1.Now()
	objMeta.DeletionTimestamp = &now
}

// EndpointsUpdateProcessor is a processor for endpoints.
type EndpointsUpdateProcessor struct{}

// SetDeleted sets the deletion timestamp for the object, if there is none already set.
func (p *EndpointsUpdateProcessor) SetDeleted(obj runtime.Object) {
	e, ok := obj.(*v1.Endpoints)
	if !ok {
		return
	}
	setDeleted(&e.ObjectMeta)
}

// ValidateUpdate checks that the provided endpoints object is valid.
func (p *EndpointsUpdateProcessor) ValidateUpdate(obj runtime.Object, state *ProcessorState) bool {
	e, ok := obj.(*v1.Endpoints)
	if !ok {
		log.WithField("object", obj).Trace("Received non-endpoints object when handling endpoint metadata.")
		return false
	}

	// kube-scheduler and kube-controller-manager endpoints (and
	// any endpoint with leader election) are
	// updated almost every second, leading to terrible noise,
	// and hence constant listener invocation. So, here we
	// ignore endpoint updates that contain changes to only leader annotation.
	// More:
	// https://github.com/kubernetes/kubernetes/issues/41635
	// https://github.com/kubernetes/kubernetes/issues/34627
	const leaderAnnotation = "control-plane.alpha.kubernetes.io/leader"
	_, exists := e.Annotations[leaderAnnotation]
	if exists {
		delete(e.Annotations, leaderAnnotation)
		e.ResourceVersion = ""
		storedMsg, exists := state.LeaderMsgs[e.UID]
		if exists {
			// Check if the message is the same as before except for the annotation.
			if reflect.DeepEqual(e, storedMsg) {
				log.
					WithField("uid", e.UID).
					Trace("Dropping message because it only mismatches on leader annotation")
				return false
			}
		} else {
			state.LeaderMsgs[e.UID] = e
		}
	}

	// Don't record the endpoint if there is no nodename.
	if len(e.Subsets) == 0 || len(e.Subsets[0].Addresses) == 0 || e.Subsets[0].Addresses[0].NodeName == nil {
		return false
	}

	return true
}

// GetStoredProtos gets the update protos that should be persisted.
func (p *EndpointsUpdateProcessor) GetStoredProtos(obj runtime.Object) ([]*storepb.K8SResource, []string) {
	e := obj.(*v1.Endpoints)

	pb, err := protoutils.EndpointsToProto(e)
	if err != nil {
		return nil, nil
	}
	storedUpdate := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Endpoints{
			Endpoints: pb,
		},
	}
	return []*storepb.K8SResource{storedUpdate}, []string{pb.Metadata.ResourceVersion}
}

// GetUpdatesToSend gets the resource updates that should be sent out to the agents, along with the agent IPs that the update should be sent to.
func (p *EndpointsUpdateProcessor) GetUpdatesToSend(obj runtime.Object, state *ProcessorState) []*OutgoingUpdate {
	e := obj.(*v1.Endpoints)

	pb, err := protoutils.EndpointsToProto(e)
	if err != nil {
		return nil
	}

	var updates []*OutgoingUpdate

	// Track all pod name and pod UIDs for the update to Kelvin.
	var allPodNames []string
	var allPodUIDs []string

	// Construct a map for each IP in the endpoints, to the associated pods.
	ipToPodNames := make(map[string][]string)
	ipToPodUIDs := make(map[string][]string)
	for _, subset := range pb.Subsets {
		for _, addr := range subset.Addresses {
			if addr.TargetRef != nil && addr.TargetRef.Kind == "Pod" {
				allPodNames = append(allPodNames, addr.TargetRef.Name)
				allPodUIDs = append(allPodUIDs, addr.TargetRef.UID)

				ip, ok := state.PodToIP[fmt.Sprintf("%s/%s", addr.TargetRef.Namespace, addr.TargetRef.Name)]
				if !ok {
					continue
				}

				ipToPodNames[ip] = append(ipToPodNames[ip], addr.TargetRef.Name)
				ipToPodUIDs[ip] = append(ipToPodUIDs[ip], addr.TargetRef.UID)
			}
		}
	}

	for ip := range ipToPodNames {
		updates = append(updates, &OutgoingUpdate{
			Update: getServiceResourceUpdateFromEndpoint(pb, ipToPodUIDs[ip], ipToPodNames[ip]),
			Topics: []string{ip},
		})
	}
	// Also send update to Kelvin.
	updates = append(updates, &OutgoingUpdate{
		Update: getServiceResourceUpdateFromEndpoint(pb, allPodUIDs, allPodNames),
		Topics: []string{KelvinUpdateChannel},
	})

	return updates
}

// ServiceUpdateProcessor is a processor for services.
type ServiceUpdateProcessor struct{}

// SetDeleted sets the deletion timestamp for the object, if there is none already set.
func (p *ServiceUpdateProcessor) SetDeleted(obj runtime.Object) {
	e, ok := obj.(*v1.Service)
	if !ok {
		return
	}
	setDeleted(&e.ObjectMeta)
}

// ValidateUpdate checks that the provided service object is valid, and casts it to the correct type.
func (p *ServiceUpdateProcessor) ValidateUpdate(obj runtime.Object, state *ProcessorState) bool {
	e, ok := obj.(*v1.Service)

	if !ok {
		log.WithField("object", obj).Trace("Received non-service object when handling service metadata.")
		return false
	}

	p.updateServiceCIDR(e, state)
	return true
}

// GetStoredProtos gets the update protos that should be persisted.
func (p *ServiceUpdateProcessor) GetStoredProtos(obj runtime.Object) ([]*storepb.K8SResource, []string) {
	e := obj.(*v1.Service)

	pb, err := protoutils.ServiceToProto(e)
	if err != nil {
		return nil, nil
	}
	storedUpdate := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Service{
			Service: pb,
		},
	}
	return []*storepb.K8SResource{storedUpdate}, []string{pb.Metadata.ResourceVersion}
}

func (p *ServiceUpdateProcessor) updateServiceCIDR(svc *v1.Service, state *ProcessorState) {
	ip := net.ParseIP(svc.Spec.ClusterIP).To16()

	// Some services don't have a ClusterIP.
	if ip == nil {
		return
	}

	if state.ServiceCIDR == nil {
		state.ServiceCIDR = &net.IPNet{IP: ip, Mask: net.CIDRMask(128, 128)}
	} else {
		for !state.ServiceCIDR.Contains(ip) {
			ones, bits := state.ServiceCIDR.Mask.Size()
			state.ServiceCIDR.Mask = net.CIDRMask(ones-1, bits)
			state.ServiceCIDR.IP = state.ServiceCIDR.IP.Mask(state.ServiceCIDR.Mask)
		}
	}
}

// GetUpdatesToSend gets the resource updates that should be sent out to the agents, along with the agent IPs that the update should be sent to.
func (p *ServiceUpdateProcessor) GetUpdatesToSend(obj runtime.Object, state *ProcessorState) []*OutgoingUpdate {
	// We don't send out service updates, they are encapsulated by endpoints updates where we have more information
	// about which nodes the service is actually running on.
	return nil
}

// PodUpdateProcessor is a processor for pods.
type PodUpdateProcessor struct{}

// SetDeleted sets the deletion timestamp for the object, if there is none already set.
func (p *PodUpdateProcessor) SetDeleted(obj runtime.Object) {
	e, ok := obj.(*v1.Pod)
	if !ok {
		return
	}
	setDeleted(&e.ObjectMeta)
}

// ValidateUpdate checks that the provided pod object is valid, and casts it to the correct type.
func (p *PodUpdateProcessor) ValidateUpdate(obj runtime.Object, state *ProcessorState) bool {
	e, ok := obj.(*v1.Pod)

	if !ok {
		log.WithField("object", obj).Trace("Received non-pod object when handling pod metadata.")
		return false
	}
	p.updatePodCIDR(e, state)

	// Create a mapping from podName -> IP. This helps us filter endpoint updates down to the relevant IPs.
	podName := fmt.Sprintf("%s/%s", e.ObjectMeta.Namespace, e.ObjectMeta.Name)
	if e.ObjectMeta.DeletionTimestamp != nil {
		delete(state.PodToIP, podName)
	} else {
		state.PodToIP[podName] = e.Status.HostIP
	}

	return true
}

// GetStoredProtos gets the update protos that should be persisted.
func (p *PodUpdateProcessor) GetStoredProtos(obj runtime.Object) ([]*storepb.K8SResource, []string) {
	e := obj.(*v1.Pod)

	var updates []*storepb.K8SResource
	var resourceVersions []string

	pb, err := protoutils.PodToProto(e)
	if err != nil {
		return updates, resourceVersions
	}

	// Also store container updates. These are saved and sent as separate updates.
	containerUpdates := GetContainerUpdatesFromPod(pb)
	for i := range containerUpdates {
		updates = append(updates, &storepb.K8SResource{
			Resource: &storepb.K8SResource_Container{
				Container: containerUpdates[i],
			},
		})
		resourceVersions = append(resourceVersions, fmt.Sprintf("%s_%d", pb.Metadata.ResourceVersion, i))
	}

	updates = append(updates, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: pb,
		},
	})
	// The pod should have a later resource version than the containers, because the containers should be sent to an agent first.
	resourceVersions = append(resourceVersions, fmt.Sprintf("%s_%d", pb.Metadata.ResourceVersion, len(containerUpdates)))
	return updates, resourceVersions
}

func (p *PodUpdateProcessor) updatePodCIDR(pod *v1.Pod, state *ProcessorState) {
	if pod.Status.PodIP == "" {
		return
	}
	mergedCIDRs, err := cidrman.MergeCIDRs(append(state.PodCIDRs, pod.Status.PodIP+"/32"))
	if err != nil {
		return
	}
	state.PodCIDRs = mergedCIDRs
}

// GetUpdatesToSend gets the resource updates that should be sent out to the agents, along with the agent IPs that the update should be sent to.
func (p *PodUpdateProcessor) GetUpdatesToSend(obj runtime.Object, state *ProcessorState) []*OutgoingUpdate {
	e := obj.(*v1.Pod)

	pb, err := protoutils.PodToProto(e)
	if err != nil {
		return nil
	}

	var updates []*OutgoingUpdate

	// Create the resource updates for the specific pod.
	// Container updates should be sent first.
	containerUpdates := GetContainerUpdatesFromPod(pb)
	for i := range containerUpdates {
		ru := &metadatapb.ResourceUpdate{
			ResourceVersion: fmt.Sprintf("%s_%d", pb.Metadata.ResourceVersion, i),
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: containerUpdates[i],
			},
		}
		// Send the update to the relevant agent and the Kelvin.
		updates = append(updates, &OutgoingUpdate{
			Update: ru,
			Topics: []string{e.Status.HostIP, KelvinUpdateChannel},
		})
	}

	// Send out the pod update to the relevant agent and Kelvin.
	podUpdate := GetResourceUpdateFromPod(pb)
	updates = append(updates, &OutgoingUpdate{
		Update: podUpdate,
		Topics: []string{e.Status.HostIP, KelvinUpdateChannel},
	})

	return updates
}

// NodeUpdateProcessor is a processor for nodes.
type NodeUpdateProcessor struct{}

// SetDeleted sets the deletion timestamp for the object, if there is none already set.
func (p *NodeUpdateProcessor) SetDeleted(obj runtime.Object) {
	e, ok := obj.(*v1.Node)
	if !ok {
		return
	}
	setDeleted(&e.ObjectMeta)
}

// ValidateUpdate checks that the provided service object is valid, and casts it to the correct type.
func (p *NodeUpdateProcessor) ValidateUpdate(obj runtime.Object, state *ProcessorState) bool {
	n, ok := obj.(*v1.Node)

	if !ok {
		log.WithField("object", obj).Trace("Received non-node object when handling node metadata.")
		return false
	}

	// Create a mapping from node -> IP. This gives us a list of all IPs in the cluster that we need to
	// send updates to.
	if n.DeletionTimestamp != nil {
		delete(state.NodeToIP, n.ObjectMeta.Name)
		return true
	}

	for _, addr := range n.Status.Addresses {
		if addr.Type == v1.NodeInternalIP {
			state.NodeToIP[n.ObjectMeta.Name] = addr.Address
			break
		}
	}

	return true
}

// GetStoredProtos gets the update protos that should be persisted.
func (p *NodeUpdateProcessor) GetStoredProtos(obj runtime.Object) ([]*storepb.K8SResource, []string) {
	e := obj.(*v1.Node)

	pb, err := protoutils.NodeToProto(e)
	if err != nil {
		return nil, nil
	}
	storedUpdate := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Node{
			Node: pb,
		},
	}
	return []*storepb.K8SResource{storedUpdate}, []string{pb.Metadata.ResourceVersion}
}

// GetUpdatesToSend gets the resource updates that should be sent out to the agents, along with the agent IPs that the update should be sent to.
func (p *NodeUpdateProcessor) GetUpdatesToSend(obj runtime.Object, state *ProcessorState) []*OutgoingUpdate {
	// Currently we don't send node updates to the agents.
	return nil
}

// NamespaceUpdateProcessor is a processor for namespaces.
type NamespaceUpdateProcessor struct{}

// SetDeleted sets the deletion timestamp for the object, if there is none already set.
func (p *NamespaceUpdateProcessor) SetDeleted(obj runtime.Object) {
	e, ok := obj.(*v1.Namespace)
	if !ok {
		return
	}
	setDeleted(&e.ObjectMeta)
}

// ValidateUpdate checks that the provided namespace object is valid, and casts it to the correct type.
func (p *NamespaceUpdateProcessor) ValidateUpdate(obj runtime.Object, state *ProcessorState) bool {
	_, ok := obj.(*v1.Namespace)

	if !ok {
		log.WithField("object", obj).Trace("Received non-namespace object when handling namespace metadata.")
		return false
	}
	return true
}

// GetStoredProtos gets the update protos that should be persisted.
func (p *NamespaceUpdateProcessor) GetStoredProtos(obj runtime.Object) ([]*storepb.K8SResource, []string) {
	e := obj.(*v1.Namespace)

	pb, err := protoutils.NamespaceToProto(e)
	if err != nil {
		return nil, nil
	}
	storedUpdate := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: pb,
		},
	}
	return []*storepb.K8SResource{storedUpdate}, []string{pb.Metadata.ResourceVersion}
}

// GetUpdatesToSend gets the resource updates that should be sent out to the agents, along with the agent IPs that the update should be sent to.
func (p *NamespaceUpdateProcessor) GetUpdatesToSend(obj runtime.Object, state *ProcessorState) []*OutgoingUpdate {
	e := obj.(*v1.Namespace)

	pb, err := protoutils.NamespaceToProto(e)
	if err != nil {
		return nil
	}

	// Send the update to all agents.
	agents := []string{KelvinUpdateChannel}
	for _, ip := range state.NodeToIP {
		agents = append(agents, ip)
	}
	return []*OutgoingUpdate{
		&OutgoingUpdate{
			Update: getResourceUpdateFromNamespace(pb),
			Topics: agents,
		},
	}
}

func formatContainerID(cid string) string {
	// Strip prefixes like docker:// or containerd://
	tokens := strings.SplitN(cid, "://", 2)
	if len(tokens) != 2 {
		if cid != "" {
			log.Info("Container ID is not in the expected format: " + cid)
		}
		return cid
	}
	return tokens[1]
}

// GetContainerUpdatesFromPod gets the container updates for the given pod.
func GetContainerUpdatesFromPod(pod *metadatapb.Pod) []*metadatapb.ContainerUpdate {
	updates := make([]*metadatapb.ContainerUpdate, len(pod.Status.ContainerStatuses))

	for i, s := range pod.Status.ContainerStatuses {
		updates[i] = &metadatapb.ContainerUpdate{
			CID:              formatContainerID(s.ContainerID),
			Name:             s.Name,
			StartTimestampNS: s.StartTimestampNS,
			StopTimestampNS:  s.StopTimestampNS,
			PodID:            pod.Metadata.UID,
			PodName:          pod.Metadata.Name,
			Namespace:        pod.Metadata.Namespace,
			ContainerState:   s.ContainerState,
			Message:          s.Message,
			Reason:           s.Reason,
		}
	}
	return updates
}

func getResourceUpdateFromNamespace(ns *metadatapb.Namespace) *metadatapb.ResourceUpdate {
	return &metadatapb.ResourceUpdate{
		ResourceVersion: ns.Metadata.ResourceVersion,
		Update: &metadatapb.ResourceUpdate_NamespaceUpdate{
			NamespaceUpdate: &metadatapb.NamespaceUpdate{
				UID:              ns.Metadata.UID,
				Name:             ns.Metadata.Name,
				StartTimestampNS: ns.Metadata.CreationTimestampNS,
				StopTimestampNS:  ns.Metadata.DeletionTimestampNS,
			},
		},
	}
}

func getServiceResourceUpdateFromEndpoint(ep *metadatapb.Endpoints, podIDs []string, podNames []string) *metadatapb.ResourceUpdate {
	update := &metadatapb.ResourceUpdate{
		ResourceVersion: ep.Metadata.ResourceVersion,
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              ep.Metadata.UID,
				Name:             ep.Metadata.Name,
				Namespace:        ep.Metadata.Namespace,
				StartTimestampNS: ep.Metadata.CreationTimestampNS,
				StopTimestampNS:  ep.Metadata.DeletionTimestampNS,
				PodIDs:           podIDs,
				PodNames:         podNames,
			},
		},
	}
	return update
}

// Stop stops processing incoming k8s metadata updates.
func (m *K8sMetadataHandler) Stop() {
	m.once.Do(func() {
		close(m.done)
	})
}
