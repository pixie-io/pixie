package controllers

import (
	"strings"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/runtime"

	protoutils "pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	"pixielabs.ai/pixielabs/src/utils"
	datapb "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

const maxAgentUpdates = 10000

// MetadataStore is the interface for our metadata store.
type MetadataStore interface {
	UpdateEndpoints(*metadatapb.Endpoints) error
	UpdatePod(*metadatapb.Pod) error
	UpdateService(*metadatapb.Service) error
	UpdateContainer(*metadatapb.ContainerInfo) error
	UpdateContainersFromPod(*metadatapb.Pod) error
	UpdateSchemas(uuid.UUID, []*metadatapb.SchemaInfo) error
	UpdateProcesses([]*metadatapb.ProcessInfo) error
	GetAgentsForHostnames(*[]string) (*[]string, error)
	AddToAgentUpdateQueue(string, string) error
	AddUpdatesToAgentQueue(string, []*metadatapb.ResourceUpdate) error
	AddToFrontOfAgentQueue(string, *metadatapb.ResourceUpdate) error
	GetFromAgentQueue(string) ([]*metadatapb.ResourceUpdate, error)
	GetAgents() (*[]datapb.AgentData, error)
	GetNodePods(hostname string) ([]*metadatapb.Pod, error)
	GetPods() ([]*metadatapb.Pod, error)
	GetContainers() ([]*metadatapb.ContainerInfo, error)
	GetNodeEndpoints(hostname string) ([]*metadatapb.Endpoints, error)
	GetEndpoints() ([]*metadatapb.Endpoints, error)
	GetServices() ([]*metadatapb.Service, error)
	GetComputedSchemas() ([]*metadatapb.SchemaInfo, error)
	GetASID() (uint32, error)
	GetProcesses([]*types.UInt128) ([]*metadatapb.ProcessInfo, error)
}

// K8sMessage is a message for K8s metadata events/updates.
type K8sMessage struct {
	Object     runtime.Object
	ObjectType string
}

// UpdateMessage is an update message for a specific hostname.
type UpdateMessage struct {
	Message   *metadatapb.ResourceUpdate
	Hostnames *[]string
}

// MetadataHandler processes messages in its channel.
type MetadataHandler struct {
	ch            chan *K8sMessage
	mds           MetadataStore
	agentUpdateCh chan *UpdateMessage
	clock         utils.Clock
}

// NewMetadataHandlerWithClock creates a new metadata handler with a clock.
func NewMetadataHandlerWithClock(mds MetadataStore, clock utils.Clock) (*MetadataHandler, error) {
	c := make(chan *K8sMessage)
	agentUpdateCh := make(chan *UpdateMessage, maxAgentUpdates)

	mh := &MetadataHandler{
		ch:            c,
		mds:           mds,
		agentUpdateCh: agentUpdateCh,
		clock:         clock,
	}

	go mh.MetadataListener()

	return mh, nil
}

// NewMetadataHandler creates a new metadata handler.
func NewMetadataHandler(mds MetadataStore) (*MetadataHandler, error) {
	clock := utils.SystemClock{}
	return NewMetadataHandlerWithClock(mds, clock)
}

// GetChannel returns the channel the MetadataHandler is listening to.
func (mh *MetadataHandler) GetChannel() chan *K8sMessage {
	return mh.ch
}

// ProcessAgentUpdates starts the goroutine for processing the agent update channel.
func (mh *MetadataHandler) ProcessAgentUpdates() {
	go mh.processAgentUpdates()
}

func (mh *MetadataHandler) processAgentUpdates() {
	for {
		more := mh.ProcessNextAgentUpdate()
		if !more {
			return
		}
	}
}

// ProcessNextAgentUpdate processes the next agent update in the agent update channel. Returns true if there are more
// requests to be processed.
func (mh *MetadataHandler) ProcessNextAgentUpdate() bool {
	// If there are too many updates, something is probably very wrong.
	if len(mh.agentUpdateCh) > maxAgentUpdates {
		log.Fatal("Failed to queue maximum number of agent updates.")
	}

	msg, more := <-mh.agentUpdateCh
	if !more {
		return more
	}

	// Apply updates.
	mh.updateAgentQueues(msg.Message, msg.Hostnames)

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
			mh.handleEndpointsMetadata(msg.Object)
		case "services":
			mh.handleServiceMetadata(msg.Object)
		case "pods":
			mh.handlePodMetadata(msg.Object)
		default:
			log.Error("Received unknown metadata message with type: " + msg.ObjectType)
		}
	}
}

// updateAgentQueues appends the resource update to the relevant agent queues. If appending to the queue has failed,
// it adds the update to the handler's retry channel.
func (mh *MetadataHandler) updateAgentQueues(updatePb *metadatapb.ResourceUpdate, hostnames *[]string) {
	agents, err := mh.mds.GetAgentsForHostnames(hostnames)

	if agents == nil || len(*agents) == 0 {
		return
	}

	update, err := updatePb.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not marshall service update message.")
		return
	}

	var failedHostnames []string
	for i, agent := range *agents {
		err = mh.mds.AddToAgentUpdateQueue(agent, string(update))
		if err != nil {
			log.WithError(err).Error("Could not write service update to agent update queue.")
			failedHostnames = append(failedHostnames, (*hostnames)[i])
		}
	}

	if len(failedHostnames) > 0 {
		mh.agentUpdateCh <- &UpdateMessage{
			Message:   updatePb,
			Hostnames: &failedHostnames,
		}
	}
}

func (mh *MetadataHandler) handleEndpointsMetadata(o runtime.Object) {
	e := o.(*v1.Endpoints)

	// Don't record the endpoint if there is no nodename.
	if len(e.Subsets) > 0 && len(e.Subsets[0].Addresses) > 0 && e.Subsets[0].Addresses[0].NodeName == nil {
		log.Info("Received endpoint with no nodename: " + e.String())
		return
	}

	pb, err := protoutils.EndpointsToProto(e)
	if err != nil {
		log.WithError(err).Fatal("Could not convert endpoints object to protobuf.")
	}
	err = mh.mds.UpdateEndpoints(pb)
	if err != nil {
		log.WithError(err).Fatal("Could not write endpoints protobuf to metadata store.")
	}

	// Add endpoint update to agent update queues.
	var hostnames []string
	for _, subset := range pb.Subsets {
		for _, addr := range subset.Addresses {
			hostnames = append(hostnames, addr.NodeName)
		}
	}

	updatePb := GetResourceUpdateFromEndpoints(pb)

	mh.agentUpdateCh <- &UpdateMessage{
		Hostnames: &hostnames,
		Message:   updatePb,
	}
}

func (mh *MetadataHandler) handlePodMetadata(o runtime.Object) {
	e := o.(*v1.Pod)

	pb, err := protoutils.PodToProto(e)
	if err != nil {
		log.WithError(err).Fatal("Could not convert pod object to protobuf.")
	}

	err = mh.mds.UpdatePod(pb)
	if err != nil {
		log.WithError(err).Fatal("Could not write pod protobuf to metadata store.")
	}

	err = mh.mds.UpdateContainersFromPod(pb)
	if err != nil {
		log.WithError(err).Fatal("Could not write container protobufs to metadata store.")
	}

	// Add pod update to agent update queue.
	hostname := []string{e.Spec.NodeName}

	// Send container updates.
	containerUpdates := GetContainerResourceUpdatesFromPod(pb)
	for _, update := range containerUpdates {
		mh.agentUpdateCh <- &UpdateMessage{
			Hostnames: &hostname,
			Message:   update,
		}
	}

	updatePb := GetResourceUpdateFromPod(pb)

	mh.agentUpdateCh <- &UpdateMessage{
		Hostnames: &hostname,
		Message:   updatePb,
	}
}

func (mh *MetadataHandler) handleServiceMetadata(o runtime.Object) {
	e := o.(*v1.Service)

	pb, err := protoutils.ServiceToProto(e)
	if err != nil {
		log.WithError(err).Fatal("Could not convert service object to protobuf.")
	}
	err = mh.mds.UpdateService(pb)
	if err != nil {
		log.WithError(err).Fatal("Could not write service protobuf to metadata store.")
	}
}

func formatContainerID(cid string) string {
	return strings.Replace(cid, "docker://", "", 1)
}

// GetResourceUpdateFromPod gets the update info from the given pod proto.
func GetResourceUpdateFromPod(pod *metadatapb.Pod) *metadatapb.ResourceUpdate {
	var containers []string
	if pod.Status.ContainerStatuses != nil {
		for _, s := range pod.Status.ContainerStatuses {
			containers = append(containers, formatContainerID(s.ContainerID))
		}
	}

	update := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:              pod.Metadata.UID,
				Name:             pod.Metadata.Name,
				Namespace:        pod.Metadata.Namespace,
				StartTimestampNS: pod.Metadata.CreationTimestampNS,
				StopTimestampNS:  pod.Metadata.DeletionTimestampNS,
				QOSClass:         pod.Status.QOSClass,
				ContainerIDs:     containers,
			},
		},
	}

	return update
}

func serviceResourceUpdateFromEndpoint(ep *metadatapb.Endpoints, pods []string) *metadatapb.ResourceUpdate {
	update := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              ep.Metadata.UID,
				Name:             ep.Metadata.Name,
				Namespace:        ep.Metadata.Namespace,
				StartTimestampNS: ep.Metadata.CreationTimestampNS,
				StopTimestampNS:  ep.Metadata.DeletionTimestampNS,
				PodIDs:           pods,
			},
		},
	}
	return update
}

// GetResourceUpdateFromEndpoints gets the update info from the given endpoint proto.
func GetResourceUpdateFromEndpoints(ep *metadatapb.Endpoints) *metadatapb.ResourceUpdate {
	var pods []string
	for _, subset := range ep.Subsets {
		for _, addr := range subset.Addresses {
			if addr.TargetRef != nil && addr.TargetRef.Kind == "Pod" {
				pods = append(pods, addr.TargetRef.UID)
			}
		}
	}

	return serviceResourceUpdateFromEndpoint(ep, pods)
}

// GetNodeResourceUpdateFromEndpoints gets the update info for a node from the given endpoint proto.
func GetNodeResourceUpdateFromEndpoints(ep *metadatapb.Endpoints, hostname string) *metadatapb.ResourceUpdate {
	var pods []string
	for _, subset := range ep.Subsets {
		for _, addr := range subset.Addresses {
			if addr.TargetRef != nil && addr.TargetRef.Kind == "Pod" && addr.NodeName == hostname {
				pods = append(pods, addr.TargetRef.UID)
			}
		}
	}

	return serviceResourceUpdateFromEndpoint(ep, pods)
}

// GetContainerResourceUpdatesFromPod gets the container updates for the given pod.
func GetContainerResourceUpdatesFromPod(pod *metadatapb.Pod) []*metadatapb.ResourceUpdate {
	updates := make([]*metadatapb.ResourceUpdate, len(pod.Status.ContainerStatuses))

	for i, s := range pod.Status.ContainerStatuses {
		updates[i] = &metadatapb.ResourceUpdate{
			Update: &metadatapb.ResourceUpdate_ContainerUpdate{
				ContainerUpdate: &metadatapb.ContainerUpdate{
					CID:              formatContainerID(s.ContainerID),
					Name:             s.Name,
					StartTimestampNS: s.StartTimestampNS,
					StopTimestampNS:  s.StopTimestampNS,
				},
			},
		}
	}
	return updates
}

// SyncPodData syncs the data in etcd according to the current active pods.
func (mh *MetadataHandler) SyncPodData(podList *v1.PodList) {
	activePods := map[string]bool{}
	activeContainers := map[string]bool{}

	currentTime := mh.clock.Now().UnixNano()

	// Create a map so that we can easily check which pods and containers are currently active by ID.
	for _, item := range podList.Items {
		activePods[string(item.ObjectMeta.UID)] = true
		for _, container := range item.Status.ContainerStatuses {
			activeContainers[formatContainerID(container.ContainerID)] = true
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
				err := mh.mds.UpdatePod(pod)
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
}

// SyncEndpointsData syncs the data in etcd according to the current active pods.
func (mh *MetadataHandler) SyncEndpointsData(epList *v1.EndpointsList) {
	activeEps := map[string]bool{}

	currentTime := mh.clock.Now().UnixNano()

	// Create a map so that we can easily check which endpoints are currently active by ID.
	for _, item := range epList.Items {
		activeEps[string(item.ObjectMeta.UID)] = true
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
			err := mh.mds.UpdateEndpoints(ep)
			if err != nil {
				log.WithError(err).Error("Could not update endpoint during sync.")
			}
		}
	}
}

// SyncServiceData syncs the data in etcd according to the current active pods.
func (mh *MetadataHandler) SyncServiceData(sList *v1.ServiceList) {
	activeServices := map[string]bool{}

	currentTime := mh.clock.Now().UnixNano()

	// Create a map so that we can easily check which services are currently active by ID.
	for _, item := range sList.Items {
		activeServices[string(item.ObjectMeta.UID)] = true
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
			err := mh.mds.UpdateService(service)
			if err != nil {
				log.WithError(err).Error("Could not update service during sync.")
			}
		}
	}
}
