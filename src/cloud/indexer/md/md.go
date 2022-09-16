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

package md

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/olivere/elastic/v7"
	"github.com/prometheus/client_golang/prometheus"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/shared/services/msgbus"
)

const (
	maxActionsPerBatch = 256
	// The period when we flush data to Elasticsearch.
	defaultFlushInterval = time.Second * 10
)

var (
	elasticFailuresCollector = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "elastic_index_failures",
		Help: "The number of failures for the vizier_id index",
	}, []string{"vizier_id"})
)

func init() {
	prometheus.MustRegister(elasticFailuresCollector)
}

// VizierIndexer run the indexer for a single vizier index.
type VizierIndexer struct {
	st        msgbus.Streamer
	es        *elastic.Client
	bulk      *elastic.BulkService
	vizierID  uuid.UUID
	orgID     uuid.UUID
	k8sUID    string
	indexName string

	sub    msgbus.PersistentSub
	quitCh chan bool
	errCh  chan error

	bulkMu            sync.Mutex
	bulkFlushInterval time.Duration
}

// NewVizierIndexerWithBulkSettings creates a new Vizier indexer with bulk settings.
func NewVizierIndexerWithBulkSettings(vizierID uuid.UUID, orgID uuid.UUID, k8sUID, indexName string, st msgbus.Streamer,
	es *elastic.Client, batchFlushInterval time.Duration) *VizierIndexer {
	return &VizierIndexer{
		st: st,
		es: es,
		// This will get automatically reset for reuse after every call to `bulk.Do`.
		bulk:              es.Bulk().Index(indexName),
		vizierID:          vizierID,
		orgID:             orgID,
		k8sUID:            k8sUID,
		indexName:         indexName,
		quitCh:            make(chan bool),
		errCh:             make(chan error),
		bulkFlushInterval: batchFlushInterval,
	}
}

// NewVizierIndexer creates a new Vizier indexer.
func NewVizierIndexer(vizierID uuid.UUID, orgID uuid.UUID, k8sUID, indexName string, st msgbus.Streamer, es *elastic.Client) *VizierIndexer {
	return NewVizierIndexerWithBulkSettings(vizierID, orgID, k8sUID, indexName, st, es, defaultFlushInterval)
}

func (v *VizierIndexer) flush() {
	v.bulkMu.Lock()
	defer v.bulkMu.Unlock()

	// Don't run if there's nothing to flush.
	if v.bulk.NumberOfActions() == 0 {
		return
	}

	_, err := v.bulk.Refresh("false").Do(context.Background())
	if err != nil {
		log.WithError(err).Error("Failed to flush bulk")
		// Record the failure in prometheus.
		elasticFailuresCollector.WithLabelValues(v.vizierID.String()).Add(1.0)
	}
}

// FlushRoutine runs the routine that handles bulk API flushing.
func (v *VizierIndexer) FlushRoutine() {
	ticker := time.NewTicker(v.bulkFlushInterval)
	defer ticker.Stop()
	for {
		select {
		case <-v.quitCh:
			return
		case <-ticker.C:
			v.flush()
		}
	}
}

// Start starts the indexer.
func (v *VizierIndexer) Start(topic string) error {
	log.
		WithField("VizierID", v.vizierID).
		WithField("ClusterUID", v.k8sUID).
		Info("Starting Indexer")

	sub, err := v.st.PersistentSubscribe(topic, v.indexName, v.streamHandler)
	if err != nil {
		return fmt.Errorf("Failed to subscribe to topic %s: %s", topic, err.Error())
	}
	v.sub = sub

	go func() {
		for {
			select {
			case <-v.quitCh:
				return
			case err = <-v.errCh:
				log.WithField("vizier", v.vizierID.String()).WithError(err).Error("Error during indexing")
			}
		}
	}()

	go v.FlushRoutine()

	return nil
}

// Stop stops the indexer.
func (v *VizierIndexer) Stop() {
	close(v.quitCh)
	err := v.sub.Close()
	if err != nil {
		log.WithError(err).Error("Failed to un-subscribe from channel")
	}
}

func namespacedName(namespace string, name string) string {
	if namespace == "" {
		return name
	}
	return fmt.Sprintf("%s/%s", namespace, name)
}

func (v *VizierIndexer) nsUpdateToEMD(u *metadatapb.ResourceUpdate, nsUpdate *metadatapb.NamespaceUpdate) *EsMDEntity {
	return &EsMDEntity{
		OrgID:              v.orgID.String(),
		VizierID:           v.vizierID.String(),
		ClusterUID:         v.k8sUID,
		UID:                nsUpdate.UID,
		Name:               nsUpdate.Name,
		Kind:               string(EsMDTypeNamespace),
		TimeStartedNS:      nsUpdate.StartTimestampNS,
		TimeStoppedNS:      nsUpdate.StopTimestampNS,
		RelatedEntityNames: []string{},
		UpdateVersion:      u.UpdateVersion,
		State:              getStateFromTimestamps(nsUpdate.StopTimestampNS),
	}
}

func podPhaseToState(podUpdate *metadatapb.PodUpdate) ESMDEntityState {
	switch podUpdate.Phase {
	case metadatapb.PENDING:
		return ESMDEntityStatePending
	case metadatapb.RUNNING:
		return ESMDEntityStateRunning
	case metadatapb.SUCCEEDED:
		return ESMDEntityStateTerminated
	case metadatapb.FAILED:
		return ESMDEntityStateFailed
	case metadatapb.TERMINATED:
		return ESMDEntityStateTerminated
	default:
		return ESMDEntityStateUnknown
	}
}

func getStateFromTimestamps(stopTimestamp int64) ESMDEntityState {
	if stopTimestamp > 0 {
		return ESMDEntityStateTerminated
	}
	return ESMDEntityStateRunning
}

func (v *VizierIndexer) podUpdateToEMD(u *metadatapb.ResourceUpdate, podUpdate *metadatapb.PodUpdate) *EsMDEntity {
	return &EsMDEntity{
		OrgID:              v.orgID.String(),
		VizierID:           v.vizierID.String(),
		ClusterUID:         v.k8sUID,
		UID:                podUpdate.UID,
		Name:               namespacedName(podUpdate.Namespace, podUpdate.Name),
		Kind:               string(EsMDTypePod),
		TimeStartedNS:      podUpdate.StartTimestampNS,
		TimeStoppedNS:      podUpdate.StopTimestampNS,
		RelatedEntityNames: []string{},
		UpdateVersion:      u.UpdateVersion,
		State:              podPhaseToState(podUpdate),
	}
}

func (v *VizierIndexer) serviceUpdateToEMD(u *metadatapb.ResourceUpdate, serviceUpdate *metadatapb.ServiceUpdate) *EsMDEntity {
	if serviceUpdate.PodIDs == nil {
		serviceUpdate.PodIDs = make([]string, 0)
	}
	return &EsMDEntity{
		OrgID:              v.orgID.String(),
		VizierID:           v.vizierID.String(),
		ClusterUID:         v.k8sUID,
		UID:                serviceUpdate.UID,
		Name:               namespacedName(serviceUpdate.Namespace, serviceUpdate.Name),
		Kind:               string(EsMDTypeService),
		TimeStartedNS:      serviceUpdate.StartTimestampNS,
		TimeStoppedNS:      serviceUpdate.StopTimestampNS,
		RelatedEntityNames: serviceUpdate.PodIDs,
		UpdateVersion:      u.UpdateVersion,
		State:              getStateFromTimestamps(serviceUpdate.StopTimestampNS),
	}
}

func (v *VizierIndexer) nodeUpdateToEMD(u *metadatapb.ResourceUpdate, nodeUpdate *metadatapb.NodeUpdate) *EsMDEntity {
	return &EsMDEntity{
		OrgID:              v.orgID.String(),
		VizierID:           v.vizierID.String(),
		ClusterUID:         v.k8sUID,
		UID:                nodeUpdate.UID,
		Name:               nodeUpdate.Name,
		Kind:               string(EsMDTypeNode),
		TimeStartedNS:      nodeUpdate.StartTimestampNS,
		TimeStoppedNS:      nodeUpdate.StopTimestampNS,
		RelatedEntityNames: []string{},
		UpdateVersion:      u.UpdateVersion,
		State:              nodeConditionToState(nodeUpdate),
	}
}

func nodeConditionToState(node *metadatapb.NodeUpdate) ESMDEntityState {
	if node.StopTimestampNS != 0 {
		return ESMDEntityStateTerminated
	}

	if node.Conditions == nil {
		return ESMDEntityStateUnknown
	}

	for _, c := range node.Conditions {
		if c.Type == metadatapb.NODE_CONDITION_READY {
			if c.Status == metadatapb.CONDITION_STATUS_TRUE {
				return ESMDEntityStateRunning
			}
		}
	}

	return ESMDEntityStatePending
}

func (v *VizierIndexer) resourceUpdateToEMD(update *metadatapb.ResourceUpdate) *EsMDEntity {
	switch update.Update.(type) {
	case *metadatapb.ResourceUpdate_NamespaceUpdate:
		return v.nsUpdateToEMD(update, update.GetNamespaceUpdate())
	case *metadatapb.ResourceUpdate_PodUpdate:
		return v.podUpdateToEMD(update, update.GetPodUpdate())
	case *metadatapb.ResourceUpdate_ServiceUpdate:
		return v.serviceUpdateToEMD(update, update.GetServiceUpdate())
	case *metadatapb.ResourceUpdate_NodeUpdate:
		return v.nodeUpdateToEMD(update, update.GetNodeUpdate())
	default:
		// We don't care about any other update types.
		// Notably containerUpdates and nodeUpdates.
		return nil
	}
}

const elasticUpdateScript = `
if (params.updateVersion <= ctx._source.updateVersion)  {
  ctx.op = 'noop';
}
ctx._source.relatedEntityNames.addAll(params.entities);
ctx._source.relatedEntityNames = ctx._source.relatedEntityNames.stream().distinct().sorted().collect(Collectors.toList());
ctx._source.timeStoppedNS = params.timeStoppedNS;
ctx._source.updateVersion = params.updateVersion;
ctx._source.state = params.state;
`

func (v *VizierIndexer) streamHandler(msg msgbus.Msg) {
	ru := metadatapb.ResourceUpdate{}
	err := ru.Unmarshal(msg.Data())
	if err != nil {
		log.WithError(err).Error("Could not unmarshal message from JetStream")
		v.errCh <- err
		err = msg.Ack()
		if err != nil {
			log.WithError(err).Error("Failed to ack JetStream msg")
		}
		return
	}

	err = v.HandleResourceUpdate(&ru)
	if err != nil {
		log.WithError(err).Error("Error handling resource update")
		v.errCh <- err
		err = msg.Ack()
		if err != nil {
			log.WithError(err).Error("Failed to ack JetStream msg")
		}

		return
	}

	err = msg.Ack()
	if err != nil {
		log.WithError(err).Error("Failed to ack JetStream msg")
	}
}

// HandleResourceUpdate indexes the resource update in elastic.
func (v *VizierIndexer) HandleResourceUpdate(update *metadatapb.ResourceUpdate) error {
	esEntity := v.resourceUpdateToEMD(update)
	if esEntity == nil { // We are not handling this resource yet.
		return nil
	}

	id := fmt.Sprintf("%s-%s-%s", v.vizierID, v.k8sUID, esEntity.UID)
	req := elastic.NewBulkUpdateRequest().
		Id(id).
		Script(
			elastic.NewScript(elasticUpdateScript).
				Param("entities", esEntity.RelatedEntityNames).
				Param("timeStoppedNS", esEntity.TimeStoppedNS).
				Param("updateVersion", esEntity.UpdateVersion).
				Param("state", esEntity.State).
				Lang("painless")).
		Upsert(esEntity)
	v.bulkMu.Lock()
	v.bulk.Add(req)
	numPending := v.bulk.NumberOfActions()
	v.bulkMu.Unlock()

	if numPending >= maxActionsPerBatch {
		v.flush()
	}

	return nil
}
