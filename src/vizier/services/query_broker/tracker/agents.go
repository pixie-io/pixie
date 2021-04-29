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

package tracker

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

const (
	updateIntervalSeconds = 5
	maxUpdatesPerResponse = 100
)

// Agents tracks the current state of running agent in the system.
// It refreshes state in the background to prevent high latencies in the critical path.
// The pupose of this is to simply orchestrate the agent state update, but not actually store it.
type Agents struct {
	mdsClient  metadatapb.MetadataServiceClient
	signingKey string

	done chan bool
	wg   sync.WaitGroup

	agentsInfo AgentsInfo
}

// NewAgents creates a new agent tracker.
func NewAgents(mdsClient metadatapb.MetadataServiceClient, signingKey string) *Agents {
	return NewAgentsWithInfo(mdsClient, signingKey, NewAgentsInfo())
}

// NewAgentsWithInfo creates an agent tracker with the AgentsInfo passed in.
func NewAgentsWithInfo(mdsClient metadatapb.MetadataServiceClient, signingKey string, agentsInfo AgentsInfo) *Agents {
	return &Agents{
		mdsClient:  mdsClient,
		signingKey: signingKey,
		done:       make(chan bool),
		agentsInfo: agentsInfo,
	}
}

// Start runs the background loop of the agent tracker.
func (a *Agents) Start() {
	log.Trace("Starting Agent Tracker.")
	a.wg.Add(1)
	defer a.wg.Done()

	doneCalled := false
	var err error

	go func() {
		for !doneCalled {
			// Retry stream when errors are encountered.
			doneCalled, err = a.runLoop()
			if doneCalled {
				return
			}
			log.WithError(err).Info("Received error running agent tracker loop. Retrying in 5 seconds.")
			// If we encounter an error, wait 5 seconds then try again.
			time.Sleep(5 * time.Second)
		}
	}()
}

// Stop stops the background loop of the agent tracker.
func (a *Agents) Stop() {
	log.Trace("Stopping Agent Tracker.")
	a.done <- true
	a.wg.Wait()
	close(a.done)
}

// GetAgentInfo returns the current agent info.
func (a *Agents) GetAgentInfo() AgentsInfo {
	return a.agentsInfo
}

func (a *Agents) runLoop() (bool, error) {
	respCh, cancel, err := a.streamUpdates()
	if err != nil {
		return false, err
	}
	defer cancel()
	for {
		select {
		case <-a.done:
			return true, nil
		case msg := <-respCh:
			if msg.Err != nil {
				return false, msg.Err
			}
			err = a.updateState(msg.Update, msg.FreshState)
			if err != nil {
				return false, err
			}
		}
	}
}

func (a *Agents) updateState(update *metadatapb.AgentUpdatesResponse, clearCurrentState bool) error {
	if clearCurrentState {
		a.agentsInfo.ClearPendingState()
	}
	return a.agentsInfo.UpdateAgentsInfo(update)
}

type updateOrError struct {
	Update *metadatapb.AgentUpdatesResponse
	Err    error
	// GetAgentUpdates first reads the full initial state and then sends only updates after that.
	// If this is the first message, then we need to start from a fresh state instead of appending
	// to the current state.
	FreshState bool
}

func (a *Agents) streamUpdates() (chan updateOrError, func(), error) {
	log.Trace("Streaming agent state.")

	claims := utils.GenerateJWTForService("metadata_tracker", "vizier")
	token, _ := utils.SignJWTClaims(claims, a.signingKey)

	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", token))
	ctx, cancel := context.WithCancel(ctx)

	req := &metadatapb.AgentUpdatesRequest{
		MaxUpdateInterval: &types.Duration{
			Seconds: updateIntervalSeconds,
		},
		MaxUpdatesPerResponse: maxUpdatesPerResponse,
	}
	resp, err := a.mdsClient.GetAgentUpdates(ctx, req)
	if err != nil {
		return nil, cancel, err
	}

	results := make(chan updateOrError)
	firstMsg := true
	go func() {
		for {
			select {
			case <-a.done:
				return
			case <-resp.Context().Done():
				return
			case <-ctx.Done():
				return
			default:
				msg, err := resp.Recv()
				results <- updateOrError{
					Err:        err,
					Update:     msg,
					FreshState: firstMsg,
				}
				if firstMsg {
					firstMsg = false
				}
				if err != nil || msg == nil {
					close(results)
					return
				}
			}
		}
	}()

	return results, cancel, err
}
