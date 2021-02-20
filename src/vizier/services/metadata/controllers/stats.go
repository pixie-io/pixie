package controllers

import (
	"fmt"
	"sync/atomic"
	"time"

	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"k8s.io/apimachinery/pkg/watch"

	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
)

// Stats about an entity type
type k8sEntityTypeStats struct {
	numUpdates int64
	totalSize  int64

	numAdded    int64
	numModified int64
	numDeleted  int64
	numBookmark int64
	numError    int64
}

// StatsHandler tracks internal stats for metadata service.
type StatsHandler struct {
	doneCh chan bool

	// stats
	numHeartbeats int64

	// Agent data info
	agentDataInfos         int64
	agentDataInfoTotalSize int64

	// Process stats
	terminatedProcesses          int64
	createdProcesses             int64
	terminatedProcessesTotalSize int64
	createdProcessesTotalSize    int64

	// K8s updates
	endpointStats  *k8sEntityTypeStats
	namespaceStats *k8sEntityTypeStats
	nodeStats      *k8sEntityTypeStats
	podStats       *k8sEntityTypeStats
	serviceStats   *k8sEntityTypeStats
}

// NewStatsHandler creates a stats handler and starts its periodic logging goroutine.
func NewStatsHandler() *StatsHandler {
	s := &StatsHandler{
		doneCh:         make(chan bool),
		endpointStats:  &k8sEntityTypeStats{},
		namespaceStats: &k8sEntityTypeStats{},
		nodeStats:      &k8sEntityTypeStats{},
		podStats:       &k8sEntityTypeStats{},
		serviceStats:   &k8sEntityTypeStats{},
	}
	go s.periodicallyLogStats(60 * time.Second)
	return s
}

// Stop stops the stats handler.
func (s *StatsHandler) Stop() {
	close(s.doneCh)
}

func (s *StatsHandler) periodicallyLogStats(period time.Duration) {
	t := time.NewTicker(period)
	defer t.Stop()
	for {
		select {
		case <-t.C:
			s.logAndFlushStats()
		case <-s.doneCh:
			return
		}
	}
}

func (s *StatsHandler) logAndFlushK8sStats(k *k8sEntityTypeStats, name string) {
	numUpdates := atomic.SwapInt64(&k.numUpdates, 0)
	totalSize := atomic.SwapInt64(&k.totalSize, 0)
	numAdded := atomic.SwapInt64(&k.numAdded, 0)
	numModified := atomic.SwapInt64(&k.numModified, 0)
	numDeleted := atomic.SwapInt64(&k.numDeleted, 0)
	numBookmark := atomic.SwapInt64(&k.numBookmark, 0)
	numError := atomic.SwapInt64(&k.numError, 0)

	log.
		WithField(fmt.Sprintf("num_%s_updates", name), numUpdates).
		WithField(fmt.Sprintf("total_%s_update_bytes", name), totalSize).
		WithField(fmt.Sprintf("num_%s_added", name), numAdded).
		WithField(fmt.Sprintf("num_%s_modified", name), numModified).
		WithField(fmt.Sprintf("num_%s_deleted", name), numDeleted).
		WithField(fmt.Sprintf("num_%s_bookmark", name), numBookmark).
		WithField(fmt.Sprintf("num_%s_error", name), numError).
		Infof("k8s %s stats", name)
}

func (s *StatsHandler) logAndFlushStats() {
	numHeartbeats := atomic.SwapInt64(&s.numHeartbeats, 0)
	agentDataInfos := atomic.SwapInt64(&s.agentDataInfos, 0)
	agentDataInfoTotalSize := atomic.SwapInt64(&s.agentDataInfoTotalSize, 0)
	terminatedProcesses := atomic.SwapInt64(&s.terminatedProcesses, 0)
	terminatedProcessesTotalSize := atomic.SwapInt64(&s.terminatedProcessesTotalSize, 0)
	createdProcesses := atomic.SwapInt64(&s.terminatedProcesses, 0)
	createdProcessesTotalSize := atomic.SwapInt64(&s.createdProcessesTotalSize, 0)

	log.
		WithField("num_heartbeats", numHeartbeats).
		WithField("num_agent_data_infos", agentDataInfos).
		WithField("total_agent_data_info_bytes", agentDataInfoTotalSize).
		WithField("num_created_processes", createdProcesses).
		WithField("total_created_processes_bytes", createdProcessesTotalSize).
		WithField("num_terminated_processes", terminatedProcesses).
		WithField("total_terminated_processes_bytes", terminatedProcessesTotalSize).
		Info("agent heartbeat stats")

	s.logAndFlushK8sStats(s.endpointStats, "endpoints")
	s.logAndFlushK8sStats(s.namespaceStats, "namespace")
	s.logAndFlushK8sStats(s.nodeStats, "node")
	s.logAndFlushK8sStats(s.podStats, "pod")
	s.logAndFlushK8sStats(s.serviceStats, "service")
}

// HandleAgentHeartbeat takes in an agent heartbeat and adds its relevant stats.
func (s *StatsHandler) HandleAgentHeartbeat(m *messages.Heartbeat) {
	atomic.AddInt64(&s.numHeartbeats, 1)

	if m.UpdateInfo == nil {
		return
	}

	// processes
	numTerminated := len(m.UpdateInfo.ProcessTerminated)
	numCreated := len(m.UpdateInfo.ProcessCreated)
	terminatedSize := int64(0)
	createdSize := int64(0)
	for _, el := range m.UpdateInfo.ProcessTerminated {
		terminatedSize += int64(proto.Size(el))
	}
	for _, el := range m.UpdateInfo.ProcessCreated {
		createdSize += int64(proto.Size(el))
	}

	atomic.AddInt64(&s.terminatedProcesses, int64(numTerminated))
	atomic.AddInt64(&s.createdProcesses, int64(numCreated))
	atomic.AddInt64(&s.terminatedProcessesTotalSize, terminatedSize)
	atomic.AddInt64(&s.createdProcessesTotalSize, createdSize)

	// data info
	if m.UpdateInfo.Data != nil {
		atomic.AddInt64(&s.agentDataInfos, 1)
		atomic.AddInt64(&s.agentDataInfoTotalSize, int64(proto.Size(m.UpdateInfo.Data)))
	}
}

// HandleK8sUpdate tracks the size and type of the k8s update, per entity type.
func (s *StatsHandler) HandleK8sUpdate(msg *K8sResourceMessage) {
	var object *k8sEntityTypeStats

	switch msg.ObjectType {
	case "endpoints":
		object = s.endpointStats
	case "namespaces":
		object = s.namespaceStats
	case "nodes":
		object = s.nodeStats
	case "pods":
		object = s.podStats
	case "services":
		object = s.serviceStats
	default:
		log.Errorf("stats didn't get an expected type, received %s", msg.ObjectType)
		return
	}

	size := proto.Size(msg.Object)
	atomic.AddInt64(&object.numUpdates, 1)
	atomic.AddInt64(&object.totalSize, int64(size))
	switch msg.EventType {
	case watch.Added:
		atomic.AddInt64(&object.numAdded, 1)
	case watch.Modified:
		atomic.AddInt64(&object.numModified, 1)
	case watch.Deleted:
		atomic.AddInt64(&object.numDeleted, 1)
	case watch.Bookmark:
		atomic.AddInt64(&object.numBookmark, 1)
	case watch.Error:
		atomic.AddInt64(&object.numError, 1)
	}
}
