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

package vizhealth

import (
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/shared/services/utils"
)

const (
	hcRetryInterval    = 2 * time.Second
	hcWatchDogInterval = 10 * time.Second
)

// Checker runs a remote health check to make sure that Vizier is in a queryable state.
type Checker struct {
	quitCh     chan bool
	signingKey string
	vzClient   vizierpb.VizierServiceClient
	wg         sync.WaitGroup

	hcRestMu         sync.Mutex
	latestHCResp     *vizierpb.HealthCheckResponse
	latestHCRespTime time.Time
}

// NewChecker creates and starts a Vizier Checker. Stop must be called when done to prevent leaking
// goroutines making requests to Vizier.
func NewChecker(signingKey string, vzClient vizierpb.VizierServiceClient) *Checker {
	c := &Checker{
		quitCh:     make(chan bool),
		signingKey: signingKey,
		vzClient:   vzClient,
	}
	c.wg.Add(1)
	go c.run()
	return c
}

func (c *Checker) run() {
	defer c.wg.Done()

	hcCount := int64(0)
	lastHCCount := int64(-1)
	// We run a watchdog to make sure the health checks are not getting blocked.
	go func() {
		t := time.NewTicker(hcWatchDogInterval)
		defer t.Stop()
		for {
			select {
			case <-c.quitCh:
				log.Trace("Quitting health check watchdog")
				return
			case <-t.C:
				current := atomic.LoadInt64(&hcCount)
				if lastHCCount == current {
					log.Trace("watchdog timeout on healthcheck")
					// Write nil b/c health check has hung.
					c.updateHCResp(nil)
				}
				lastHCCount = current
			}
		}
	}()

	failCount := 0
	handleFailure := func(err error) {
		log.WithError(err).Info("failed vizier health check, will restart healthcheck")
		failCount++
		if failCount > 1 {
			// If it fails more than once then update the response to nil.
			// This provides some hysteresis for transient failures.
			c.updateHCResp(nil)
		}
	}

	for {
		failCount++
		select {
		case <-c.quitCh:
			log.Trace("Quitting health monitor")
			return
		default:
		}

		claims := utils.GenerateJWTForService("cloud_conn", "vizier")
		token, _ := utils.SignJWTClaims(claims, c.signingKey)

		ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
			fmt.Sprintf("bearer %s", token))
		ctx, cancel := context.WithCancel(ctx)
		defer cancel()

		resp, err := c.vzClient.HealthCheck(ctx, &vizierpb.HealthCheckRequest{})
		if err != nil {
			handleFailure(err)
			time.Sleep(hcRetryInterval)
			continue
		}
		failCount = 0
		go func() {
			defer cancel()
			for {
				hc, err := resp.Recv()
				if err != nil || hc == nil {
					handleFailure(err)
					return
				}
				atomic.AddInt64(&hcCount, 1)
				c.updateHCResp(hc)
			}
		}()

		select {
		case <-c.quitCh:
			log.Trace("Quitting health monitor")
			cancel()
			return
		case <-resp.Context().Done():
			handleFailure(resp.Context().Err())
			time.Sleep(hcRetryInterval)
			continue
		}
	}
}

// Stop the checker.
func (c *Checker) Stop() {
	close(c.quitCh)
	c.wg.Wait()
	c.updateHCResp(nil)
}

func (c *Checker) updateHCResp(resp *vizierpb.HealthCheckResponse) {
	c.hcRestMu.Lock()
	defer c.hcRestMu.Unlock()

	c.latestHCResp = resp
	c.latestHCRespTime = time.Now()
}

// GetStatus returns the current status and the last check time.
func (c *Checker) GetStatus() (time.Time, error) {
	c.hcRestMu.Lock()
	defer c.hcRestMu.Unlock()
	checkTime := c.latestHCRespTime

	if c.latestHCResp == nil {
		return checkTime, fmt.Errorf("missing vizier healthcheck response")
	}

	if c.latestHCResp.Status.Code == 0 {
		return checkTime, nil
	}
	return checkTime, fmt.Errorf("vizier healthcheck is failing: %s", c.latestHCResp.Status.Message)
}
