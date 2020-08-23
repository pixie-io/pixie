package tracker

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/metadata"
	utils2 "pixielabs.ai/pixielabs/src/shared/services/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
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

	agentsInfo   AgentsInfo
	agentsInfoMu sync.Mutex
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
			log.WithError(err).Errorf("Received error running agent tracker loop. Retrying in 5 seconds.")
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
	a.agentsInfoMu.Lock()
	defer a.agentsInfoMu.Unlock()
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
			err = a.updateState(msg.Update, msg.ClearPrevState)
			if err != nil {
				return false, err
			}
		}
	}
}

func (a *Agents) updateState(update *metadatapb.AgentUpdatesResponse, clearCurrentState bool) error {
	a.agentsInfoMu.Lock()
	defer a.agentsInfoMu.Unlock()
	if clearCurrentState {
		a.agentsInfo.ClearState()
	}
	return a.agentsInfo.UpdateAgentsInfo(update.AgentUpdates, update.AgentSchemas, update.AgentSchemasUpdated)
}

type updateOrError struct {
	Update *metadatapb.AgentUpdatesResponse
	Err    error
	// GetAgentUpdates first reads the full initial state and then sends only updates after that.
	// As a result, on the first message we need to clear our current agent state to get rid of
	// stale agent information.
	ClearPrevState bool
}

func (a *Agents) streamUpdates() (chan (updateOrError), func(), error) {
	log.Trace("Streaming agent state.")

	claims := utils2.GenerateJWTForService("metadata_tracker")
	token, _ := utils2.SignJWTClaims(claims, a.signingKey)

	ctx := context.Background()
	ctx = metadata.AppendToOutgoingContext(context.Background(), "authorization",
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
					Err:            err,
					Update:         msg,
					ClearPrevState: firstMsg,
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
