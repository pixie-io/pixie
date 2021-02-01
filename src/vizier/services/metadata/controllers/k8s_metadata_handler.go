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
	"k8s.io/apimachinery/pkg/runtime"
	k8stypes "k8s.io/apimachinery/pkg/types"
	"k8s.io/apimachinery/pkg/watch"

	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

// K8sMessage is a message for K8s metadata events/updates.
type K8sMessage struct {
	Object     runtime.Object
	ObjectType string
	EventType  watch.EventType
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
	// ValidateUpdate checks whether the update is valid and should be further processed.
	ValidateUpdate(runtime.Object, *ProcessorState) bool
	// GetStoredProtos gets the protos that should be persisted in the data store, derived from
	// the given update.
	GetStoredProtos(runtime.Object) (updates []*storepb.K8SResource, resourceVersions []string)
}

// ProcessorState is data that should be shared across update processors. It can contain that needs
// to be cached across all updates, such as CIDRs or tracking leader election messages.
type ProcessorState struct {
	// This is a cache of all the leader election message. This is used to avoid sending excessive
	// endpoints updates.
	LeaderMsgs  map[k8stypes.UID]*v1.Endpoints
	ServiceCIDR *net.IPNet // This is the service CIDR block; it is inferred from all observed service IPs.
	PodCIDRs    []string   // The pod CIDRs in the cluster, inferred from each node's reported pod CIDR.
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
	state := ProcessorState{LeaderMsgs: leaderMsgs, PodCIDRs: make([]string, 0)}
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

			// Check that the update is valid and should be handled.
			valid := processor.ValidateUpdate(msg.Object, &m.state)
			if !valid {
				continue
			}
			// Persist the update in the data store.
			updates, rvs := processor.GetStoredProtos(msg.Object)
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

// EndpointsUpdateProcessor is a processor for endpoints.
type EndpointsUpdateProcessor struct{}

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

// ServiceUpdateProcessor is a processor for services.
type ServiceUpdateProcessor struct{}

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

// PodUpdateProcessor is a processor for pods.
type PodUpdateProcessor struct{}

// ValidateUpdate checks that the provided pod object is valid, and casts it to the correct type.
func (p *PodUpdateProcessor) ValidateUpdate(obj runtime.Object, state *ProcessorState) bool {
	e, ok := obj.(*v1.Pod)

	if !ok {
		log.WithField("object", obj).Trace("Received non-pod object when handling pod metadata.")
		return false
	}
	p.updatePodCIDR(e, state)
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
	updates = append(updates, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: pb,
		},
	})
	resourceVersions = append(resourceVersions, pb.Metadata.ResourceVersion)

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

// NodeUpdateProcessor is a processor for nodes.
type NodeUpdateProcessor struct{}

// ValidateUpdate checks that the provided service object is valid, and casts it to the correct type.
func (p *NodeUpdateProcessor) ValidateUpdate(obj runtime.Object, state *ProcessorState) bool {
	_, ok := obj.(*v1.Node)

	if !ok {
		log.WithField("object", obj).Trace("Received non-node object when handling node metadata.")
		return false
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

// NamespaceUpdateProcessor is a processor for namespaces.
type NamespaceUpdateProcessor struct{}

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

// Stop stops processing incoming k8s metadata updates.
func (m *K8sMetadataHandler) Stop() {
	m.once.Do(func() {
		close(m.done)
	})
}
