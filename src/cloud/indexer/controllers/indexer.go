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
	"github.com/nats-io/stan.go"
	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/cloud/indexer/md"
	"px.dev/pixie/src/cloud/shared/vzutils"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
)

// The topic on which updates are written to.
const indexerMetadataTopic = "MetadataIndex"

// Indexer manages the state for which clusters are already being indexed.
type Indexer struct {
	clusters   map[string]*md.VizierIndexer // Map from cluster UID->indexer.
	clustersMu sync.Mutex

	sc stan.Conn
	es *elastic.Client

	watcher *vzutils.Watcher
}

// NewIndexer creates a new Vizier indexer. This is a wrapper around the Vizier Watcher, which starts the indexer
// for any active viziers.
func NewIndexer(nc *nats.Conn, vzmgrClient vzmgrpb.VZMgrServiceClient, sc stan.Conn, es *elastic.Client, fromShardID string, toShardID string) (*Indexer, error) {
	watcher, err := vzutils.NewWatcher(nc, vzmgrClient, fromShardID, toShardID)
	if err != nil {
		return nil, err
	}

	i := &Indexer{
		clusters: make(map[string]*md.VizierIndexer),
		watcher:  watcher,
		sc:       sc,
		es:       es,
	}

	err = watcher.RegisterVizierHandler(i.handleVizier)
	if err != nil {
		return nil, err
	}

	return i, nil
}

// Stop stops the indexer.
func (i *Indexer) Stop() {
	i.clustersMu.Lock()
	defer i.clustersMu.Unlock()

	// Stop the watcher.
	i.watcher.Stop()

	// Stop the indexers for the individual clusters.
	for _, v := range i.clusters {
		if v != nil {
			v.Stop()
		}
	}
}

func (i *Indexer) handleVizier(id uuid.UUID, orgID uuid.UUID, uid string) error {
	i.clustersMu.Lock()
	defer i.clustersMu.Unlock()

	if val, ok := i.clusters[uid]; ok && val != nil {
		log.WithField("UID", uid).Info("Already running indexer for cluster")
		return nil
	}

	// Start indexer.
	vzIndexer := md.NewVizierIndexer(id, orgID, uid, i.sc, i.es)
	i.clusters[uid] = vzIndexer
	go vzIndexer.Run(fmt.Sprintf("%s.%s", indexerMetadataTopic, uid))

	return nil
}
