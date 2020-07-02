package controllers

import (
	"fmt"
	"reflect"
	"strconv"
	"strings"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/runtime"
	k8stypes "k8s.io/apimachinery/pkg/types"
	"k8s.io/apimachinery/pkg/watch"

	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	"pixielabs.ai/pixielabs/src/utils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

const maxSubscriberUpdates = 10000

// MetadataStore is the interface for our metadata store.
type MetadataStore interface {
	GetServiceCIDR() string
	GetPodCIDRs() []string
	GetAgent(agentID uuid.UUID) (*agentpb.Agent, error)
	GetAgentIDForHostnamePair(hnPair *HostnameIPPair) (string, error)
	DeleteAgent(agentID uuid.UUID) error
	CreateAgent(agentID uuid.UUID, a *agentpb.Agent) error
	UpdateAgent(agentID uuid.UUID, a *agentpb.Agent) error
	GetAgents() ([]*agentpb.Agent, error)
	GetAgentsDataInfo() (map[uuid.UUID]*messagespb.AgentDataInfo, error)
	UpdateAgentDataInfo(agentID uuid.UUID, dataInfo *messagespb.AgentDataInfo) error
	GetAgentsForHostnamePairs(*[]*HostnameIPPair) ([]string, error)
	GetASID() (uint32, error)
	GetKelvinIDs() ([]string, error)
	GetComputedSchemas() ([]*metadatapb.SchemaInfo, error)
	UpdateSchemas(agentID uuid.UUID, schemas []*metadatapb.SchemaInfo) error
	UpdateProcesses(processes []*metadatapb.ProcessInfo) error
	GetProcesses(upids []*types.UInt128) ([]*metadatapb.ProcessInfo, error)
	GetPods() ([]*metadatapb.Pod, error)
	UpdatePod(*metadatapb.Pod, bool) error
	GetNodePods(hostname *HostnameIPPair) ([]*metadatapb.Pod, error)
	GetEndpoints() ([]*metadatapb.Endpoints, error)
	UpdateEndpoints(*metadatapb.Endpoints, bool) error
	GetNodeEndpoints(hostname *HostnameIPPair) ([]*metadatapb.Endpoints, error)
	GetServices() ([]*metadatapb.Service, error)
	UpdateService(*metadatapb.Service, bool) error
	GetNodes() ([]*metadatapb.Node, error)
	UpdateNode(*metadatapb.Node, bool) error
	GetNamespaces() ([]*metadatapb.Namespace, error)
	UpdateNamespace(*metadatapb.Namespace, bool) error
	GetContainers() ([]*metadatapb.ContainerInfo, error)
	UpdateContainer(*metadatapb.ContainerInfo) error
	UpdateContainersFromPod(*metadatapb.Pod, bool) error
	GetMetadataUpdates(hostname *HostnameIPPair) ([]*metadatapb.ResourceUpdate, error)
	AddResourceVersion(string, *metadatapb.MetadataObject) error
	UpdateSubscriberResourceVersion(string, string) error
	GetSubscriberResourceVersion(string) (string, error)
	GetMetadataUpdatesForHostname(*HostnameIPPair, string, string) ([]*metadatapb.ResourceUpdate, error)
	GetHostnameIPPairFromPodName(string, string) (*HostnameIPPair, error)
}

// MetadataSubscriber is a consumer of metadata updates.
type MetadataSubscriber interface {
	HandleUpdate(*UpdateMessage)
}

// K8sMessage is a message for K8s metadata events/updates.
type K8sMessage struct {
	Object     runtime.Object
	ObjectType string
	EventType  watch.EventType
}

// UpdateMessage is an update message for a specific hostname.
type UpdateMessage struct {
	Message      *metadatapb.ResourceUpdate
	Hostnames    []*HostnameIPPair
	NodeSpecific bool // Specifies whether the update should only be sent to a specific node.
}

// MetadataHandler processes messages in its channel.
type MetadataHandler struct {
	ch                chan *K8sMessage
	mds               MetadataStore
	subscriberUpdates chan *UpdateMessage
	clock             utils.Clock
	isLeader          *bool
	subscribers       []MetadataSubscriber

	// This is a cache of all the leader election message.
	leaderMsgs map[k8stypes.UID]*v1.Endpoints
}

// NewMetadataHandlerWithClock creates a new metadata handler with a clock.
func NewMetadataHandlerWithClock(mds MetadataStore, isLeader *bool, clock utils.Clock) (*MetadataHandler, error) {
	c := make(chan *K8sMessage)
	subscriberUpdates := make(chan *UpdateMessage, maxSubscriberUpdates)

	mh := &MetadataHandler{
		ch:                c,
		mds:               mds,
		subscriberUpdates: subscriberUpdates,
		clock:             clock,
		isLeader:          isLeader,
		leaderMsgs:        make(map[k8stypes.UID]*v1.Endpoints),
		subscribers:       make([]MetadataSubscriber, 0),
	}

	go mh.MetadataListener()

	return mh, nil
}

// NewMetadataHandler creates a new metadata handler.
func NewMetadataHandler(mds MetadataStore, isLeader *bool) (*MetadataHandler, error) {
	clock := utils.SystemClock{}
	return NewMetadataHandlerWithClock(mds, isLeader, clock)
}

// GetChannel returns the channel the MetadataHandler is listening to.
func (mh *MetadataHandler) GetChannel() chan *K8sMessage {
	return mh.ch
}

// AddSubscriber adds a subscriber to metadata updates.
func (mh *MetadataHandler) AddSubscriber(sub MetadataSubscriber) {
	mh.subscribers = append(mh.subscribers, sub)
}

// ProcessSubscriberUpdates starts the goroutine for processing the subscriber update channel.
func (mh *MetadataHandler) ProcessSubscriberUpdates() {
	go mh.processSubscriberUpdates()
}

func (mh *MetadataHandler) processSubscriberUpdates() {
	for {
		more := mh.ProcessNextSubscriberUpdate()
		if !more {
			return
		}
	}
}

// ProcessNextSubscriberUpdate processes the next subscriber update in the update channel. Returns true if there are more
// requests to be processed.
func (mh *MetadataHandler) ProcessNextSubscriberUpdate() bool {
	// If there are too many updates, something is probably very wrong.
	if len(mh.subscriberUpdates) > maxSubscriberUpdates {
		log.Fatal("Failed to queue maximum number of subscriber updates.")
	}

	msg, more := <-mh.subscriberUpdates
	if !more {
		return more
	}

	if !*mh.isLeader {
		return true
	}

	// Send update to subscribers.
	for _, sub := range mh.subscribers {
		sub.HandleUpdate(msg)
	}

	return more
}

// MetadataListener handles any updates on the metadata channel.
func (mh *MetadataHandler) MetadataListener() {
	for {
		msg, more := <-mh.ch
		if !more {
			return
		}

		switch msg.ObjectType {
		case "endpoints":
			mh.handleEndpointsMetadata(msg.Object, msg.EventType)
		case "services":
			mh.handleServiceMetadata(msg.Object, msg.EventType)
		case "pods":
			mh.handlePodMetadata(msg.Object, msg.EventType)
		case "nodes":
			mh.handleNodeMetadata(msg.Object, msg.EventType)
		case "namespaces":
			mh.handleNamespaceMetadata(msg.Object, msg.EventType)
		default:
			log.Error("Received unknown metadata message with type: " + msg.ObjectType)
		}
	}
}

func (mh *MetadataHandler) updateEndpoints(e *v1.Endpoints, deleted bool) (*metadatapb.Endpoints, error) {
	// Don't record the endpoint if there is no nodename.
	if len(e.Subsets) > 0 && len(e.Subsets[0].Addresses) > 0 && e.Subsets[0].Addresses[0].NodeName == nil {
		log.Info("Received endpoint with no nodename: " + e.String())
		return nil, nil
	}

	pb, err := protoutils.EndpointsToProto(e)
	if err != nil {
		return nil, err
	}
	err = mh.mds.UpdateEndpoints(pb, deleted)
	if err != nil {
		return nil, err
	}

	return pb, nil
}

func (mh *MetadataHandler) handleEndpointsMetadata(o runtime.Object, eventType watch.EventType) {
	e, ok := o.(*v1.Endpoints)
	if !ok {
		log.WithField("object", o).Error("Received non-endpoints object when handling endpoint metadata.")
		return
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
		storedMsg, exists := mh.leaderMsgs[e.UID]
		if exists {
			// Check if the message is the same as before except for the annotation.
			if reflect.DeepEqual(e, storedMsg) {
				log.
					WithField("uid", e.UID).
					Trace("Dropping message because it only mismatches on leader annotation")
				return
			}
		} else {
			mh.leaderMsgs[e.UID] = e
		}
	}

	pb, err := mh.updateEndpoints(e, eventType == watch.Deleted)
	if err != nil {
		log.WithError(err).Fatal("Could not write endpoints update")
	}
	if pb == nil {
		return
	}

	// Add endpoint update to agent update queues.
	for _, subset := range pb.Subsets {
		for _, addr := range subset.Addresses {
			if addr.TargetRef.Kind != "Pod" {
				continue
			}
			hnPair, err := mh.mds.GetHostnameIPPairFromPodName(addr.TargetRef.Name, addr.TargetRef.Namespace)

			if err != nil {
				continue
			}

			if hnPair == nil {
				log.WithField("podName", addr.TargetRef.Name).WithField("namespace", addr.TargetRef.Namespace).Info("Could not find hostnameIPPair for pod")
				continue
			}

			updatePb := GetNodeResourceUpdateFromEndpoints(pb, hnPair, mh.mds)
			mh.subscriberUpdates <- &UpdateMessage{
				Hostnames:    []*HostnameIPPair{hnPair},
				Message:      updatePb,
				NodeSpecific: true,
			}
		}
	}
	// Add endpoint update for Kelvins.
	updatePb := GetNodeResourceUpdateFromEndpoints(pb, nil, mh.mds)
	mh.subscriberUpdates <- &UpdateMessage{
		Hostnames:    []*HostnameIPPair{},
		Message:      updatePb,
		NodeSpecific: false,
	}
}

func (mh *MetadataHandler) updatePod(e *v1.Pod, deleted bool) (*metadatapb.Pod, error) {
	pb, err := protoutils.PodToProto(e)
	if err != nil {
		return nil, err
	}

	err = mh.mds.UpdatePod(pb, deleted)
	if err != nil {
		return nil, err
	}

	err = mh.mds.UpdateContainersFromPod(pb, deleted)
	if err != nil {
		return nil, err
	}

	return pb, nil
}

func (mh *MetadataHandler) handlePodMetadata(o runtime.Object, eventType watch.EventType) {
	e, ok := o.(*v1.Pod)

	if !ok {
		log.WithField("object", o).Error("Received non-pod object when handling pod metadata.")
		return
	}

	pb, err := mh.updatePod(e, eventType == watch.Deleted)
	if err != nil {
		log.WithError(err).Fatal("Could not write pod update")
	}

	// Add pod update to agent update queue.
	hostname := []*HostnameIPPair{
		&HostnameIPPair{Hostname: "", IP: e.Status.HostIP},
	}

	// Send container updates.
	containerUpdates := GetContainerResourceUpdatesFromPod(pb)
	for _, update := range containerUpdates {
		mh.subscriberUpdates <- &UpdateMessage{
			Hostnames: hostname,
			Message:   update,
		}
	}

	updatePb := GetResourceUpdateFromPod(pb)

	mh.subscriberUpdates <- &UpdateMessage{
		Hostnames: hostname,
		Message:   updatePb,
	}
}

func (mh *MetadataHandler) updateService(e *v1.Service, deleted bool) (*metadatapb.Service, error) {
	pb, err := protoutils.ServiceToProto(e)
	if err != nil {
		return nil, err
	}
	err = mh.mds.UpdateService(pb, deleted)
	if err != nil {
		return nil, err
	}
	return pb, err
}

func (mh *MetadataHandler) handleServiceMetadata(o runtime.Object, eventType watch.EventType) {
	e, ok := o.(*v1.Service)

	if !ok {
		log.WithField("object", o).Error("Received non-service object when handling service metadata.")
		return
	}

	_, err := mh.updateService(e, eventType == watch.Deleted)
	if err != nil {
		log.WithError(err).Fatal("Could not write service update")
	}
}

func (mh *MetadataHandler) updateNamespace(e *v1.Namespace, deleted bool) (*metadatapb.Namespace, error) {
	pb, err := protoutils.NamespaceToProto(e)
	if err != nil {
		return nil, err
	}
	err = mh.mds.UpdateNamespace(pb, deleted)
	if err != nil {
		return nil, err
	}
	return pb, err
}

func (mh *MetadataHandler) handleNamespaceMetadata(o runtime.Object, eventType watch.EventType) {
	e, ok := o.(*v1.Namespace)

	if !ok {
		log.WithField("object", o).Error("Received non-namespace object when handling namespace metadata.")
		return
	}

	pb, err := mh.updateNamespace(e, eventType == watch.Deleted)
	if err != nil {
		log.WithError(err).Fatal("Could not write namespace update")
	}

	// Send namespace update to subscribers.
	updatePb := GetResourceUpdateFromNamespace(pb)
	mh.subscriberUpdates <- &UpdateMessage{
		Hostnames:    []*HostnameIPPair{},
		Message:      updatePb,
		NodeSpecific: false,
	}
}

func (mh *MetadataHandler) handleNodeMetadata(o runtime.Object, eType watch.EventType) {
	e, ok := o.(*v1.Node)

	if !ok {
		log.WithField("object", o).Error("Received non-node object when handling node metadata.")
		return
	}

	_, err := mh.updateNode(e, eType == watch.Deleted)
	if err != nil {
		log.WithError(err).Fatal("Could not write node update")
	}

	// TODO(michelle): Update aggregate PodCIDRs.
}

func (mh *MetadataHandler) updateNode(e *v1.Node, deleted bool) (*metadatapb.Node, error) {
	pb, err := protoutils.NodeToProto(e)
	if err != nil {
		return nil, err
	}
	err = mh.mds.UpdateNode(pb, deleted)
	if err != nil {
		return nil, err
	}
	return pb, err
}

func formatContainerID(cid string) string {
	// Strip prefixes like docker:// or containerd://
	tokens := strings.SplitN(cid, "://", 2)
	if len(tokens) != 2 {
		if cid != "" {
			log.Error("Container ID is not in the expected format: " + cid)
		}
		return cid
	}
	return tokens[1]
}

// GetResourceUpdateFromNamespace gets the update info from the given namespace proto.
func GetResourceUpdateFromNamespace(ns *metadatapb.Namespace) *metadatapb.ResourceUpdate {
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

// GetResourceUpdateFromPod gets the update info from the given pod proto.
func GetResourceUpdateFromPod(pod *metadatapb.Pod) *metadatapb.ResourceUpdate {
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
		ResourceVersion: fmt.Sprintf("%s_%d", pod.Metadata.ResourceVersion, len(containerIDs)),
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

func serviceResourceUpdateFromEndpoint(ep *metadatapb.Endpoints, podIDs []string, podNames []string) *metadatapb.ResourceUpdate {
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

// GetResourceUpdateFromEndpoints gets the update info from the given endpoint proto.
func GetResourceUpdateFromEndpoints(ep *metadatapb.Endpoints) *metadatapb.ResourceUpdate {
	var podIDs []string
	var podNames []string
	for _, subset := range ep.Subsets {
		for _, addr := range subset.Addresses {
			if addr.TargetRef != nil && addr.TargetRef.Kind == "Pod" {
				podIDs = append(podIDs, addr.TargetRef.UID)
				podNames = append(podNames, addr.TargetRef.Name)
			}
		}
	}

	return serviceResourceUpdateFromEndpoint(ep, podIDs, podNames)
}

// GetNodeResourceUpdateFromEndpoints gets the update info for a node from the given endpoint proto.
func GetNodeResourceUpdateFromEndpoints(ep *metadatapb.Endpoints, hnPair *HostnameIPPair, mds MetadataStore) *metadatapb.ResourceUpdate {
	var podIDs []string
	var podNames []string
	for _, subset := range ep.Subsets {
		for _, addr := range subset.Addresses {
			if addr.TargetRef != nil && addr.TargetRef.Kind == "Pod" {
				podPair, err := mds.GetHostnameIPPairFromPodName(addr.TargetRef.Name, addr.TargetRef.Namespace)
				if err != nil || podPair == nil {
					continue
				}
				if hnPair == nil || (podPair.IP == hnPair.IP) {
					podIDs = append(podIDs, addr.TargetRef.UID)
					podNames = append(podNames, addr.TargetRef.Name)
				}
			}
		}
	}

	return serviceResourceUpdateFromEndpoint(ep, podIDs, podNames)
}

// GetContainerResourceUpdatesFromPod gets the container updates for the given pod.
func GetContainerResourceUpdatesFromPod(pod *metadatapb.Pod) []*metadatapb.ResourceUpdate {
	updates := make([]*metadatapb.ResourceUpdate, len(pod.Status.ContainerStatuses))

	for i, s := range pod.Status.ContainerStatuses {
		updates[i] = &metadatapb.ResourceUpdate{
			ResourceVersion: fmt.Sprintf("%s_%d", pod.Metadata.ResourceVersion, i),
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: &metadatapb.ContainerUpdate{
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
				},
			},
		}
	}
	return updates
}

// SyncPodData syncs the data in etcd according to the current active pods.
func (mh *MetadataHandler) SyncPodData(podList *v1.PodList) int {
	activePods := map[string]bool{}
	activeContainers := map[string]bool{}

	currentTime := mh.clock.Now().UnixNano()
	rv := 0
	// Create a map so that we can easily check which pods and containers are currently active by ID.
	for _, item := range podList.Items {
		i, err := strconv.Atoi(item.ObjectMeta.ResourceVersion)
		if err != nil {
			log.WithError(err).Error("Could not get RV from pod")
		}
		if i > rv {
			rv = i
		}
		activePods[string(item.ObjectMeta.UID)] = true
		for _, container := range item.Status.ContainerStatuses {
			activeContainers[formatContainerID(container.ContainerID)] = true
		}
		// Update pod/containers in metadata store.
		_, err = mh.updatePod(&item, false)
		if err != nil {
			log.WithField("pod", item).WithError(err).Error("Could not update pod in metadata store during sync")
		}
	}

	// Find all pods in etcd.
	pods, err := mh.mds.GetPods()
	if err != nil {
		log.WithError(err).Error("Could not get all pods from etcd.")
	}

	for _, pod := range pods {
		_, exists := activePods[pod.Metadata.UID]
		// If there a pod in etcd that is not active, and is not marked as dead, mark it as dead.
		if !exists {
			touched := false
			if pod.Metadata.DeletionTimestampNS == 0 {
				log.Info("Mark pod as dead: " + string(pod.Metadata.UID))
				pod.Metadata.DeletionTimestampNS = currentTime
				touched = true
			}
			for _, s := range pod.Status.ContainerStatuses {
				if s.StopTimestampNS == 0 {
					log.Info("Mark pod's container as dead: " + string(formatContainerID(s.ContainerID)))
					s.StopTimestampNS = currentTime
					touched = true
				}
			}
			if touched {
				err := mh.mds.UpdatePod(pod, false)
				if err != nil {
					log.WithError(err).Error("Could not update pod during sync.")
				}
			}
		}
	}

	containers, err := mh.mds.GetContainers()
	if err != nil {
		log.WithError(err).Error("Could not get all containers from etcd.")
	}

	for _, container := range containers {
		_, exists := activeContainers[container.UID]
		// If there a container in etcd that is not active, and is not marked as dead, mark it as dead.
		if !exists && container.StopTimestampNS == 0 {
			log.Info("Mark container as dead: " + string(container.UID))
			container.StopTimestampNS = currentTime
			err := mh.mds.UpdateContainer(container)
			if err != nil {
				log.WithError(err).Error("Could not update container during sync.")
			}
		}
	}

	return rv
}

// SyncEndpointsData syncs the data in etcd according to the current active pods.
func (mh *MetadataHandler) SyncEndpointsData(epList *v1.EndpointsList) int {
	activeEps := map[string]bool{}

	currentTime := mh.clock.Now().UnixNano()
	rv := 0
	// Create a map so that we can easily check which endpoints are currently active by ID.
	for _, item := range epList.Items {
		i, err := strconv.Atoi(item.ObjectMeta.ResourceVersion)
		if err != nil {
			log.WithError(err).Error("Could not get RV from pod")
		}
		if i > rv {
			rv = i
		}
		activeEps[string(item.ObjectMeta.UID)] = true

		// Update endpoint in metadata store.
		_, err = mh.updateEndpoints(&item, false)
		if err != nil {
			log.WithField("endpoint", item).WithError(err).Error("Could not update endpoint in metadata store during sync")
		}
	}

	// Find all endpoints in etcd.
	eps, err := mh.mds.GetEndpoints()
	if err != nil {
		log.WithError(err).Error("Could not get all endpoints from etcd.")
	}

	for _, ep := range eps {
		_, exists := activeEps[ep.Metadata.UID]
		// If there an endpoint in etcd that is not active, and is not marked as dead, mark it as dead.
		if !exists && ep.Metadata.DeletionTimestampNS == 0 {
			ep.Metadata.DeletionTimestampNS = currentTime
			err := mh.mds.UpdateEndpoints(ep, false)
			if err != nil {
				log.WithError(err).Error("Could not update endpoint during sync.")
			}
		}
	}

	return rv
}

// SyncServiceData syncs the data in etcd according to the current active pods.
func (mh *MetadataHandler) SyncServiceData(sList *v1.ServiceList) int {
	activeServices := map[string]bool{}

	currentTime := mh.clock.Now().UnixNano()
	rv := 0
	// Create a map so that we can easily check which services are currently active by ID.
	for _, item := range sList.Items {
		i, err := strconv.Atoi(item.ObjectMeta.ResourceVersion)
		if err != nil {
			log.WithError(err).Error("Could not get RV from pod")
		}
		if i > rv {
			rv = i
		}
		activeServices[string(item.ObjectMeta.UID)] = true

		// Update service in metadata store.
		_, err = mh.updateService(&item, false)
		if err != nil {
			log.WithField("service", item).WithError(err).Error("Could not update service in metadata store during sync")
		}
	}

	// Find all services in etcd.
	services, err := mh.mds.GetServices()
	if err != nil {
		log.WithError(err).Error("Could not get all services from etcd.")
	}

	for _, service := range services {
		_, exists := activeServices[service.Metadata.UID]
		// If there a service in etcd that is not active, and is not marked as dead, mark it as dead.
		if !exists && service.Metadata.DeletionTimestampNS == 0 {
			service.Metadata.DeletionTimestampNS = currentTime
			err := mh.mds.UpdateService(service, false)
			if err != nil {
				log.WithError(err).Error("Could not update service during sync.")
			}
		}
	}

	return rv
}

// SyncNamespaceData syncs the data in etcd according to the current active namespaces.
func (mh *MetadataHandler) SyncNamespaceData(sList *v1.NamespaceList) int {
	activeNamespaces := map[string]bool{}

	currentTime := mh.clock.Now().UnixNano()
	rv := 0
	// Create a map so that we can easily check which namespaces are currently active by ID.
	for _, item := range sList.Items {
		i, err := strconv.Atoi(item.ObjectMeta.ResourceVersion)
		if err != nil {
			log.WithError(err).Error("Could not get RV from pod")
		}
		if i > rv {
			rv = i
		}
		activeNamespaces[string(item.ObjectMeta.UID)] = true

		// Update namespace in metadata store.
		_, err = mh.updateNamespace(&item, false)
		if err != nil {
			log.WithField("namespace", item).WithError(err).Error("Could not update namespace in metadata store during sync")
		}
	}

	// Find all namespaces in etcd.
	namespaces, err := mh.mds.GetNamespaces()
	if err != nil {
		log.WithError(err).Error("Could not get all namespaces from etcd.")
	}

	for _, namespace := range namespaces {
		_, exists := activeNamespaces[namespace.Metadata.UID]
		// If there a namespace in etcd that is not active, and is not marked as dead, mark it as dead.
		if !exists && namespace.Metadata.DeletionTimestampNS == 0 {
			namespace.Metadata.DeletionTimestampNS = currentTime
			err := mh.mds.UpdateNamespace(namespace, false)
			if err != nil {
				log.WithError(err).Error("Could not update namespace during sync.")
			}
		}
	}

	return rv
}

// SyncNodeData syncs the data in etcd according to the current active nodes.
func (mh *MetadataHandler) SyncNodeData(sList *v1.NodeList) int {
	activeNodes := map[string]bool{}

	currentTime := mh.clock.Now().UnixNano()
	rv := 0
	// Create a map so that we can easily check which nodes are currently active by ID.
	for _, item := range sList.Items {
		log.Info(item)
		i, err := strconv.Atoi(item.ObjectMeta.ResourceVersion)
		if err != nil {
			log.WithError(err).Error("Could not get RV from node")
		}
		if i > rv {
			rv = i
		}
		activeNodes[string(item.ObjectMeta.UID)] = true

		// Update node in metadata store.
		_, err = mh.updateNode(&item, false)
		if err != nil {
			log.WithField("node", item).WithError(err).Error("Could not update node in metadata store during sync")
		}
	}

	// Find all nodes in etcd.
	nodes, err := mh.mds.GetNodes()
	if err != nil {
		log.WithError(err).Error("Could not get all nodes from etcd.")
	}

	for _, node := range nodes {
		_, exists := activeNodes[node.Metadata.UID]
		// If there a node in etcd that is not active, and is not marked as dead, mark it as dead.
		if !exists && node.Metadata.DeletionTimestampNS == 0 {
			node.Metadata.DeletionTimestampNS = currentTime
			err := mh.mds.UpdateNode(node, false)
			if err != nil {
				log.WithError(err).Error("Could not update node during sync.")
			}
		}
	}

	// TODO(michelle): Update aggregate pod CIDRs from active nodes.

	return rv
}
