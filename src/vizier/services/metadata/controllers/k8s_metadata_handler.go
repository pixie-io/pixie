package controllers

import (
	"fmt"
	"net"
	"reflect"
	"strings"
	"sync"
	"time"

	"github.com/EvilSuperstars/go-cidrman"
	"github.com/cenkalti/backoff"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	k8stypes "k8s.io/apimachinery/pkg/types"
	"k8s.io/apimachinery/pkg/watch"

	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

// KelvinUpdateTopic is the topic that all kelvins updates are sent on.
const KelvinUpdateTopic = "all"

// K8sMetadataUpdateChannel is the channel where metadata updates are sent.
const K8sMetadataUpdateChannel = "K8sUpdates"

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

// StoredUpdate is a K8s resource update that should be persisted with its update version.
type StoredUpdate struct {
	// Update is the resource update that should be stored.
	Update *storepb.K8SResource
	// UpdateVersion is the update version of the update.
	UpdateVersion int64
}

// K8sMetadataStore handles storing and fetching any data related to K8s resources.
type K8sMetadataStore interface {
	// AddResourceUpdateForTopic stores the given resource with its associated updateVersion for 24h.
	AddResourceUpdateForTopic(updateVersion int64, topic string, resource *storepb.K8SResourceUpdate) error
	// AddResourceUpdate stores a resource update that is applicable to all topics.
	AddResourceUpdate(updateVersion int64, resource *storepb.K8SResourceUpdate) error
	// AddFullResourceUpdate stores full resource update with the given update version.
	AddFullResourceUpdate(updateversion int64, resource *storepb.K8SResource) error
	// FetchFullResourceUpdates gets the full resource updates from the `from` update version, to the `to`
	// update version (exclusive).
	FetchFullResourceUpdates(from int64, to int64) ([]*storepb.K8SResource, error)
	// FetchResourceUpdates gets the resource updates from the `from` update version, to the `to`
	// update version (exclusive).
	FetchResourceUpdates(topic string, from int64, to int64) ([]*storepb.K8SResourceUpdate, error)

	// GetUpdateVersion gets the last update version sent on a topic.
	GetUpdateVersion(topic string) (int64, error)
	// SetUpdateVersion sets the last update version sent on a topic.
	SetUpdateVersion(topic string, uv int64) error
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
	GetStoredProtos(runtime.Object) []*storepb.K8SResource
	// IsNodeScoped returns whether this update is scoped to specific nodes, or should be sent to all nodes.
	IsNodeScoped() bool
	// GetUpdatesToSend gets all of the updates that should be sent to the agents, along with the relevant IPs that
	// the updates should be sent to.
	GetUpdatesToSend([]*StoredUpdate, *ProcessorState) []*OutgoingUpdate
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

func (m *K8sMetadataHandler) mustGetCurrentUpdateVersion() int64 {
	// Get the current update version. We can't send any updates until we have the current update version,
	// so should keep retrying with an exponential backoff.
	var currRV int64
	var err error
	getRV := func() error {
		currRV, err = m.mds.GetUpdateVersion(KelvinUpdateTopic)
		return err
	}
	backOffOpts := backoff.NewExponentialBackOff()
	backOffOpts.InitialInterval = 30 * time.Second
	backOffOpts.Multiplier = 2
	backOffOpts.MaxElapsedTime = 10 * time.Minute
	err = backoff.Retry(getRV, backOffOpts)
	if err != nil {
		log.WithError(err).Fatal("Could not get current update version")
	}

	return currRV
}

func (m *K8sMetadataHandler) processUpdates() {
	currUV := m.mustGetCurrentUpdateVersion()
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
			updates := processor.GetStoredProtos(update)
			if updates == nil {
				continue
			}
			storedProtos := make([]*StoredUpdate, len(updates))
			for i, u := range updates {
				currUV++
				storedProtos[i] = &StoredUpdate{
					Update:        updates[i],
					UpdateVersion: currUV,
				}

				err := m.mds.AddFullResourceUpdate(currUV, u)
				if err != nil {
					log.WithError(err).Error("Failed to store resource update")
				}
			}

			// Send the update to the agents.
			for _, u := range processor.GetUpdatesToSend(storedProtos, &m.state) {
				u.Update.PrevUpdateVersion = 0
				storedUpdate := &storepb.K8SResourceUpdate{
					Update: u.Update,
				}
				if processor.IsNodeScoped() {
					for _, t := range u.Topics {
						err := m.mds.AddResourceUpdateForTopic(u.Update.UpdateVersion, t, storedUpdate)
						if err != nil {
							log.WithError(err).Error("Failed to store resource update")
						}
					}
				} else {
					err := m.mds.AddResourceUpdate(u.Update.UpdateVersion, storedUpdate)
					if err != nil {
						log.WithError(err).Error("Failed to store resource update")
					}
				}

				for _, t := range u.Topics {
					updateToSend := u.Update
					// Set the previous update version on the update.
					prevUV, err := m.mds.GetUpdateVersion(t)
					if err != nil || prevUV == 0 {
						log.WithError(err).Trace("Failed to get update version for topic")
						// If we cannot get a prevRV from the db, the agent won't know what
						// update came before this update. Setting the prevRV
						// to the current update version, which they definitely don't have yet,
						// will force the agent to rerequest any possible missing updates, just to be safe.
						prevUV = updateToSend.UpdateVersion
					}
					updateToSend.PrevUpdateVersion = prevUV
					// Set the new update version.
					err = m.mds.SetUpdateVersion(t, updateToSend.UpdateVersion)
					if err != nil {
						log.WithError(err).Trace("Failed to set current update version for topic")
						// If we fail to set the current update version, the topic may miss an update
						// without realizing it.
					}
					err = m.sendUpdate(updateToSend, t)
					if err != nil {
						// If this happens, the agent will rerequest the update as a missing update when it
						// receives the next update.
						log.WithError(err).Trace("Failed to send update to agent")
					}
				}
			}
		}
	}
}

func getK8sUpdateChannel(topic string) string {
	if topic == "" {
		topic = KelvinUpdateTopic
	}
	return fmt.Sprintf("%s/%s", K8sMetadataUpdateChannel, topic)
}

func (m *K8sMetadataHandler) sendUpdate(update *metadatapb.ResourceUpdate, topic string) error {
	channel := getK8sUpdateChannel(topic)

	msg := &messages.VizierMessage{
		Msg: &messages.VizierMessage_K8SMetadataMessage{
			K8SMetadataMessage: &messages.K8SMetadataMessage{
				Msg: &messages.K8SMetadataMessage_K8SMetadataUpdate{
					K8SMetadataUpdate: update,
				},
			},
		},
	}

	b, err := msg.Marshal()
	if err != nil {
		return err
	}

	err = m.conn.Publish(channel, b)
	if err != nil {
		log.WithError(err).Trace("Could not publish message to NATS.")
		return err
	}
	return nil
}

// GetUpdatesForIP gets all known resource updates for the IP in the given range.
func (m *K8sMetadataHandler) GetUpdatesForIP(ip string, from int64, to int64) ([]*metadatapb.ResourceUpdate, error) {
	if ip == "" { // If no IP is specified, caller is asking for all updates.
		ip = KelvinUpdateTopic
	}

	if to == 0 { // Caller is requesting up to latest update. Get the number of the last update that was sent out.
		lastUpdate, err := m.mds.GetUpdateVersion(KelvinUpdateTopic)
		if err != nil {
			return nil, err
		}
		to = lastUpdate + 1
	}

	// Get all updates within range for the given topic.
	allUpdates, err := m.mds.FetchResourceUpdates(ip, from, to)
	if err != nil {
		return nil, err
	}

	updates := make([]*metadatapb.ResourceUpdate, len(allUpdates))
	var currVersion int64

	// For each resource update, get the update(s) that should be sent over NATS.
	for i, u := range allUpdates {
		u.Update.PrevUpdateVersion = currVersion
		currVersion = u.Update.UpdateVersion
		updates[i] = u.Update
	}

	return updates, nil
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

// IsNodeScoped returns whether this update is scoped to specific nodes, or should be sent to all nodes.
func (p *EndpointsUpdateProcessor) IsNodeScoped() bool {
	return true
}

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
		e.ResourceVersion = "0"
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
func (p *EndpointsUpdateProcessor) GetStoredProtos(obj runtime.Object) []*storepb.K8SResource {
	e := obj.(*v1.Endpoints)

	pb, err := protoutils.EndpointsToProto(e)
	if err != nil {
		return nil
	}
	storedUpdate := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Endpoints{
			Endpoints: pb,
		},
	}
	return []*storepb.K8SResource{storedUpdate}
}

// GetUpdatesToSend gets the resource updates that should be sent out to the agents, along with the agent IPs that the update should be sent to.
func (p *EndpointsUpdateProcessor) GetUpdatesToSend(storedUpdates []*StoredUpdate, state *ProcessorState) []*OutgoingUpdate {
	if len(storedUpdates) == 0 {
		return nil
	}

	pb := storedUpdates[0].Update.GetEndpoints()
	rv := storedUpdates[0].UpdateVersion

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
			Update: getServiceResourceUpdateFromEndpoint(pb, rv, ipToPodUIDs[ip], ipToPodNames[ip]),
			Topics: []string{ip},
		})
	}
	// Also send update to Kelvin.
	updates = append(updates, &OutgoingUpdate{
		Update: getServiceResourceUpdateFromEndpoint(pb, rv, allPodUIDs, allPodNames),
		Topics: []string{KelvinUpdateTopic},
	})

	return updates
}

// ServiceUpdateProcessor is a processor for services.
type ServiceUpdateProcessor struct{}

// IsNodeScoped returns whether this update is scoped to specific nodes, or should be sent to all nodes.
func (p *ServiceUpdateProcessor) IsNodeScoped() bool {
	return false
}

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
func (p *ServiceUpdateProcessor) GetStoredProtos(obj runtime.Object) []*storepb.K8SResource {
	e := obj.(*v1.Service)

	pb, err := protoutils.ServiceToProto(e)
	if err != nil {
		return nil
	}
	storedUpdate := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Service{
			Service: pb,
		},
	}
	return []*storepb.K8SResource{storedUpdate}
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
func (p *ServiceUpdateProcessor) GetUpdatesToSend(storedUpdates []*StoredUpdate, state *ProcessorState) []*OutgoingUpdate {
	// We don't send out service updates, they are encapsulated by endpoints updates where we have more information
	// about which nodes the service is actually running on.
	return nil
}

// PodUpdateProcessor is a processor for pods.
type PodUpdateProcessor struct{}

// IsNodeScoped returns whether this update is scoped to specific nodes, or should be sent to all nodes.
func (p *PodUpdateProcessor) IsNodeScoped() bool {
	return true
}

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
func (p *PodUpdateProcessor) GetStoredProtos(obj runtime.Object) []*storepb.K8SResource {
	e := obj.(*v1.Pod)

	var updates []*storepb.K8SResource

	pb, err := protoutils.PodToProto(e)
	if err != nil {
		return updates
	}

	// Also store container updates. These are saved and sent as separate updates.
	containerUpdates := GetContainerUpdatesFromPod(pb)
	for i := range containerUpdates {
		updates = append(updates, &storepb.K8SResource{
			Resource: &storepb.K8SResource_Container{
				Container: containerUpdates[i],
			},
		})
	}

	// The pod should have a later update version than the containers, because the containers should be sent to an agent first.
	updates = append(updates, &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: pb,
		},
	})
	return updates
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
func (p *PodUpdateProcessor) GetUpdatesToSend(storedUpdates []*StoredUpdate, state *ProcessorState) []*OutgoingUpdate {
	if len(storedUpdates) == 0 {
		return nil
	}

	var updates []*OutgoingUpdate

	for _, u := range storedUpdates {
		containerUpdate := u.Update.GetContainer()
		if containerUpdate != nil {
			ru := &metadatapb.ResourceUpdate{
				UpdateVersion: u.UpdateVersion,
				Update: &metadatapb.ResourceUpdate_ContainerUpdate{
					ContainerUpdate: containerUpdate,
				},
			}
			// Send the update to the relevant agent and the Kelvin.
			topics := []string{KelvinUpdateTopic}

			podName := fmt.Sprintf("%s/%s", containerUpdate.Namespace, containerUpdate.PodName)
			if hostIP, ok := state.PodToIP[podName]; ok {
				topics = append(topics, hostIP)
			}

			updates = append(updates, &OutgoingUpdate{
				Update: ru,
				Topics: topics,
			})
			continue
		}
		podUpdate := u.Update.GetPod()
		if podUpdate != nil {
			updates = append(updates, &OutgoingUpdate{
				Update: getResourceUpdateFromPod(podUpdate, u.UpdateVersion),
				Topics: []string{podUpdate.Status.HostIP, KelvinUpdateTopic},
			})
		}
	}

	return updates
}

// NodeUpdateProcessor is a processor for nodes.
type NodeUpdateProcessor struct{}

// IsNodeScoped returns whether this update is scoped to specific nodes, or should be sent to all nodes.
func (p *NodeUpdateProcessor) IsNodeScoped() bool {
	return true
}

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
func (p *NodeUpdateProcessor) GetStoredProtos(obj runtime.Object) []*storepb.K8SResource {
	e := obj.(*v1.Node)

	pb, err := protoutils.NodeToProto(e)
	if err != nil {
		return nil
	}
	storedUpdate := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Node{
			Node: pb,
		},
	}
	return []*storepb.K8SResource{storedUpdate}
}

// GetUpdatesToSend gets the resource updates that should be sent out to the agents, along with the agent IPs that the update should be sent to.
func (p *NodeUpdateProcessor) GetUpdatesToSend(updates []*StoredUpdate, state *ProcessorState) []*OutgoingUpdate {
	// Currently we don't send node updates to the agents.
	return nil
}

// NamespaceUpdateProcessor is a processor for namespaces.
type NamespaceUpdateProcessor struct{}

// IsNodeScoped returns whether this update is scoped to specific nodes, or should be sent to all nodes.
func (p *NamespaceUpdateProcessor) IsNodeScoped() bool {
	return false
}

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
func (p *NamespaceUpdateProcessor) GetStoredProtos(obj runtime.Object) []*storepb.K8SResource {
	e := obj.(*v1.Namespace)

	pb, err := protoutils.NamespaceToProto(e)
	if err != nil {
		return nil
	}
	storedUpdate := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: pb,
		},
	}
	return []*storepb.K8SResource{storedUpdate}
}

// GetUpdatesToSend gets the resource updates that should be sent out to the agents, along with the agent IPs that the update should be sent to.
func (p *NamespaceUpdateProcessor) GetUpdatesToSend(storedUpdates []*StoredUpdate, state *ProcessorState) []*OutgoingUpdate {
	if len(storedUpdates) == 0 {
		return nil
	}

	pb := storedUpdates[0].Update.GetNamespace()
	rv := storedUpdates[0].UpdateVersion

	// Send the update to all agents.
	agents := []string{KelvinUpdateTopic}
	for _, ip := range state.NodeToIP {
		agents = append(agents, ip)
	}
	return []*OutgoingUpdate{
		&OutgoingUpdate{
			Update: getResourceUpdateFromNamespace(pb, rv),
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

func getResourceUpdateFromNamespace(ns *metadatapb.Namespace, uv int64) *metadatapb.ResourceUpdate {
	return &metadatapb.ResourceUpdate{
		UpdateVersion: uv,
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

func getServiceResourceUpdateFromEndpoint(ep *metadatapb.Endpoints, uv int64, podIDs []string, podNames []string) *metadatapb.ResourceUpdate {
	update := &metadatapb.ResourceUpdate{
		UpdateVersion: uv,
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

func getResourceUpdateFromPod(pod *metadatapb.Pod, uv int64) *metadatapb.ResourceUpdate {
	var containerIDs []string
	var containerNames []string
	if pod.Status.ContainerStatuses != nil {
		for _, s := range pod.Status.ContainerStatuses {
			containerIDs = append(containerIDs, formatContainerID(s.ContainerID))
			containerNames = append(containerNames, s.Name)
		}
	}

	var podName, hostname string
	if pod.Spec != nil {
		podName = pod.Spec.NodeName
		hostname = pod.Spec.Hostname
	}

	update := &metadatapb.ResourceUpdate{
		UpdateVersion: uv,
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:              pod.Metadata.UID,
				Name:             pod.Metadata.Name,
				Namespace:        pod.Metadata.Namespace,
				StartTimestampNS: pod.Metadata.CreationTimestampNS,
				StopTimestampNS:  pod.Metadata.DeletionTimestampNS,
				QOSClass:         pod.Status.QOSClass,
				ContainerIDs:     containerIDs,
				ContainerNames:   containerNames,
				Phase:            pod.Status.Phase,
				Conditions:       pod.Status.Conditions,
				NodeName:         podName,
				Hostname:         hostname,
				PodIP:            pod.Status.PodIP,
				HostIP:           pod.Status.HostIP,
				Message:          pod.Status.Message,
				Reason:           pod.Status.Reason,
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
