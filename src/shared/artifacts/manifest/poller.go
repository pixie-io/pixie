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

package manifest

import (
	"bytes"
	"context"
	"sync"
	"time"

	"cloud.google.com/go/storage"
	log "github.com/sirupsen/logrus"
)

// Poller polls for manifest changes, and calls a callback whenever a new manifest is uploaded.
type Poller interface {
	Start() error
	Stop()
}

// CallbackFn is the type for the callback that will be called whenever the manifest changes.
type CallbackFn func(*ArtifactManifest) error

type pollerImpl struct {
	client       *storage.Client
	bucket       string
	manifestPath string
	period       time.Duration
	cb           CallbackFn

	wg      sync.WaitGroup
	stopCh  chan struct{}
	lastMD5 []byte
}

// NewPoller creates a new Poller to poll for manifest changes.
func NewPoller(client *storage.Client, bucket string, manifestPath string, pollPeriod time.Duration, cb CallbackFn) Poller {
	return &pollerImpl{
		client:       client,
		bucket:       bucket,
		manifestPath: manifestPath,
		period:       pollPeriod,
		cb:           cb,
		stopCh:       make(chan struct{}),
	}
}

// Start synchronously gets the manifest once, and then starts a goroutine running the poller.
func (p *pollerImpl) Start() error {
	if err := p.poll(); err != nil {
		return err
	}
	p.wg.Add(1)
	go p.run()
	return nil
}

// Stop stops the poller.
func (p *pollerImpl) Stop() {
	close(p.stopCh)
	p.wg.Wait()
}

func (p *pollerImpl) run() {
	defer p.wg.Done()
	t := time.NewTicker(p.period)
	for {
		select {
		case <-t.C:
			if err := p.poll(); err != nil {
				log.WithError(err).Error("failed to poll for manifest changes")
			}
		case <-p.stopCh:
			return
		}
	}
}

func (p *pollerImpl) poll() error {
	ctx := context.Background()
	obj := p.client.Bucket(p.bucket).Object(p.manifestPath)

	attrs, err := obj.Attrs(ctx)
	if err != nil {
		return err
	}
	// If the hash hasn't changed, we skip downloading the manifest.
	if bytes.Equal(attrs.MD5, p.lastMD5) {
		return nil
	}

	p.lastMD5 = attrs.MD5

	r, err := obj.NewReader(ctx)
	if err != nil {
		return err
	}
	defer r.Close()

	m, err := ReadArtifactManifest(r)
	if err != nil {
		return err
	}
	if err := p.cb(m); err != nil {
		return err
	}
	return nil
}
