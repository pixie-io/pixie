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
	"context"
	"sync/atomic"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	clientv3 "go.etcd.io/etcd/client/v3"
)

const (
	defragBytes     = 50000000      // Begin defragging @ 500MB.
	defragFrequency = 1 * time.Hour // The minimum amount of times we should have between defrags.
)

// EtcdManager manages state for the given etcd instance.
type EtcdManager struct {
	client     *clientv3.Client
	defragging atomic.Value
	lastDefrag time.Time

	quitCh chan bool
	timer  *time.Ticker
}

// NewEtcdManager creates a new etcd manager.
func NewEtcdManager(client *clientv3.Client) *EtcdManager {
	return &EtcdManager{
		client: client,
		quitCh: make(chan bool),
	}
}

// Run periodically checks the size of the etcd db and defrags if necessary.
func (m *EtcdManager) Run() {
	m.defragging.Store(false)
	m.timer = time.NewTicker(5 * time.Minute)

	go func() {
		// Periodically check etcd state, and its memory usage.
		for {
			select {
			case _, ok := <-m.quitCh:
				if !ok {
					return
				}
			case <-m.timer.C:
				resp, err := m.client.Status(context.Background(), viper.GetString("md_etcd_server"))
				if err != nil {
					log.WithError(err).Error("Failed to get etcd members")
					continue
				}
				log.WithField("dbSizeBytes", resp.DbSize).Info("Etcd state")

				now := time.Now()
				elapsed := now.Sub(m.lastDefrag)

				if resp.DbSize > defragBytes && elapsed > defragFrequency {
					log.Info("Starting defrag")
					m.defragging.Store(true)

					defragResp, err := m.client.Defragment(context.Background(), viper.GetString("md_etcd_server"))
					if err != nil {
						log.WithError(err).Error("Failed to defrag")
					} else {
						log.WithField("resp", defragResp).Info("Finished defragging")
					}
					m.lastDefrag = time.Now()

					m.defragging.Store(false)
				}
			}
		}
	}()
}

// Stop stops the etcd manager from periodically checking and defragging etcd.
func (m *EtcdManager) Stop() {
	m.timer.Stop()
	close(m.quitCh)
}

// IsDefragging returns whether etcd is currently running a defrag.
func (m *EtcdManager) IsDefragging() bool {
	return m.defragging.Load().(bool)
}
