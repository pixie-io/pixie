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

package controller

import (
	"fmt"
	"sync"
	"time"

	"github.com/jmoiron/sqlx"
	log "github.com/sirupsen/logrus"
)

const (
	// With 5 second heartbeats, this will be 4 missed heart beats.
	durationBeforeDisconnect = 20 * time.Second
	// How often to update the database.
	updateInterval = 5 * time.Second
)

// StatusMonitor is responsible for maintaining status information of vizier clusters.
// It has a routine that is periodically invoked.
type StatusMonitor struct {
	db     *sqlx.DB
	quitCh chan struct{}
	once   sync.Once
}

// NewStatusMonitor creates a new StatusMonitor operating on the passed in DB and starts it.
func NewStatusMonitor(db *sqlx.DB) *StatusMonitor {
	sm := &StatusMonitor{
		db:     db,
		quitCh: make(chan struct{}),
	}
	sm.start()
	return sm
}

func (s *StatusMonitor) start() {
	go func() {
		tick := time.NewTicker(updateInterval)
		defer tick.Stop()

		for {
			select {
			case <-s.quitCh:
				return
			case <-tick.C:
				s.UpdateDBEntries()
			}
		}
	}()
}

// Stop kills the status monitor.
func (s *StatusMonitor) Stop() {
	s.once.Do(func() {
		close(s.quitCh)
	})
}

// UpdateDBEntries updates the database status.
func (s *StatusMonitor) UpdateDBEntries() {
	query := `
     UPDATE
       vizier_cluster_info
     SET
       status='DISCONNECTED',
       address=''
     WHERE last_heartbeat < NOW() - INTERVAL '%f seconds' AND status != 'UPDATING';`

	// Variable substitution does not seem to work for intervals. Since we control this entire
	// query and input data it should be safe to add the value to the query using
	// a format directive.
	query = fmt.Sprintf(query, durationBeforeDisconnect.Seconds())

	res, err := s.db.Exec(query)
	if err != nil {
		log.WithError(err).Error("Failed to update database, ignoring (will retry in next tick)")
		return
	}
	rowCount, _ := res.RowsAffected()
	log.WithField("entries_update", rowCount).Info("Heartbeat Update Complete")
}
