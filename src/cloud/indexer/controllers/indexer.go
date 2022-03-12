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

package controllers

import (
	"fmt"
	"sync"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/cloud/indexer/md"
	"px.dev/pixie/src/cloud/shared/vzutils"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/services/msgbus"
)

// The topic on which updates are written to.
const indexerMetadataTopic = "MetadataIndex"

type concurrentIndexersMap struct {
	unsafeMap map[string]*md.VizierIndexer
	mapMu     sync.RWMutex
}

func (c *concurrentIndexersMap) read(uid string) *md.VizierIndexer {
	c.mapMu.RLock()
	defer c.mapMu.RUnlock()
	return c.unsafeMap[uid]
}

func (c *concurrentIndexersMap) write(uid string, vz *md.VizierIndexer) {
	c.mapMu.Lock()
	defer c.mapMu.Unlock()
	c.unsafeMap[uid] = vz
}

func (c *concurrentIndexersMap) values() []*md.VizierIndexer {
	c.mapMu.RLock()
	defer c.mapMu.RUnlock()
	allIndexers := make([]*md.VizierIndexer, len(c.unsafeMap))
	i := 0
	for _, vz := range c.unsafeMap {
		allIndexers[i] = vz
		i++
	}
	return allIndexers
}

// Indexer manages the state for which clusters are already being indexed.
type Indexer struct {
	clusters *concurrentIndexersMap // Map from cluster UID->indexer.

	st        msgbus.Streamer
	es        *elastic.Client
	indexName string

	watcher *vzutils.Watcher
}

// NewIndexer creates a new Vizier indexer. This is a wrapper around the Vizier Watcher, which starts the indexer
// for any active viziers.
func NewIndexer(nc *nats.Conn, vzmgrClient vzmgrpb.VZMgrServiceClient, st msgbus.Streamer, es *elastic.Client, indexName, fromShardID, toShardID string) (*Indexer, error) {
	watcher, err := vzutils.NewWatcher(nc, vzmgrClient, fromShardID, toShardID)
	if err != nil {
		return nil, err
	}

	i := &Indexer{
		clusters:  &concurrentIndexersMap{unsafeMap: make(map[string]*md.VizierIndexer)},
		watcher:   watcher,
		st:        st,
		es:        es,
		indexName: indexName,
	}

	err = watcher.RegisterVizierHandler(i.handleVizier)
	if err != nil {
		return nil, err
	}
	return i, nil
}

// Stop stops the indexer.
func (i *Indexer) Stop() {
	// Stop the watcher.
	i.watcher.Stop()

	// Stop the indexers for the individual clusters.
	for _, v := range i.clusters.values() {
		v.Stop()
	}
}

func (i *Indexer) handleVizier(id uuid.UUID, orgID uuid.UUID, uid string) error {
	if val := i.clusters.read(uid); val != nil {
		log.WithField("UID", uid).Info("Already running indexer for cluster")
		return nil
	}

	// Start indexer.
	vzIndexer := md.NewVizierIndexer(id, orgID, uid, i.indexName, i.st, i.es)
	err := vzIndexer.Start(fmt.Sprintf("%s.%s", indexerMetadataTopic, uid))
	if err != nil {
		log.WithField("UID", uid).WithError(err).Error("Could not set up Vizier watcher for metadata updates")
		return err
	}

	i.clusters.write(uid, vzIndexer)
	return nil
}
