package tracker

import (
	"context"
	"fmt"
	"sync"
	"time"

	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/metadata"
	utils2 "pixielabs.ai/pixielabs/src/shared/services/utils"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

const (
	agentMetadataRefreshInterval = 5 * time.Second
	agentMetadataRequestTimeout  = 2 * time.Second
)

// Agents tracks the current state of running agent in the system.
// It refreshes state in the background to prevent high latencies in the critical path.
// The pupose of this is to simply orchestrate the agent state update, but not actually store it.
type Agents struct {
	mdsClient  metadatapb.MetadataServiceClient
	signingKey string

	done chan bool
	wg   sync.WaitGroup

	agentsInfo   *AgentsInfo
	agentsInfoMu sync.Mutex
}

// NewAgents creates a new agent tracker.
func NewAgents(mdsClient metadatapb.MetadataServiceClient, signingKey string) *Agents {
	return &Agents{
		mdsClient:  mdsClient,
		signingKey: signingKey,
		done:       make(chan bool),
	}
}

// Start runs the background loop of the agent tracker.
func (a *Agents) Start() {
	log.Trace("Starting Agent Tracker.")
	a.wg.Add(1)
	go a.runLoop()
}

// Stop stops the background loop of the agent tracker.
func (a *Agents) Stop() {
	log.Trace("Stopping Agent Tracker.")
	close(a.done)
	a.wg.Wait()
}

// GetAgentInfo returns the current agent info.
func (a *Agents) GetAgentInfo() *AgentsInfo {
	a.agentsInfoMu.Lock()
	defer a.agentsInfoMu.Unlock()
	return a.agentsInfo
}

func (a *Agents) runLoop() {
	defer a.wg.Done()
	t := time.NewTicker(agentMetadataRefreshInterval)
	defer t.Stop()
	a.refreshState()
	for {
		select {
		case <-a.done:
			return
		case <-t.C:
			a.refreshState()
		}
	}
}

func (a *Agents) refreshState() {
	log.Trace("Refreshing agent tracker state.")
	handleError := func(err error) {
		log.WithError(err).Error("Failed to check metadata state")
		a.agentsInfoMu.Lock()
		defer a.agentsInfoMu.Unlock()
		a.agentsInfo = nil
	}

	claims := utils2.GenerateJWTForService("metadata_tracker")
	token, _ := utils2.SignJWTClaims(claims, a.signingKey)

	ctx := context.Background()
	ctx = metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", token))
	ctx, cancel := context.WithTimeout(ctx, agentMetadataRequestTimeout)
	defer cancel()

	agentInfosReq := &metadatapb.AgentInfoRequest{}
	agentInfosResp, err := a.mdsClient.GetAgentInfo(ctx, agentInfosReq)
	if err != nil {
		handleError(err)
		return
	}

	mdsAgentTableMetadataResp, err := a.mdsClient.GetAgentTableMetadata(ctx, &metadatapb.AgentTableMetadataRequest{})
	if err != nil {
		handleError(err)
		return
	}

	var schema *schemapb.Schema
	// We currently assume the schema is shared across all agents.
	// In the unusual case where there are no agents, we can still run Kelvin-only
	// queries, so in that case we call out for the schemas directly.
	if len(mdsAgentTableMetadataResp.MetadataByAgent) > 0 {
		schema = mdsAgentTableMetadataResp.MetadataByAgent[0].Schema
	} else {
		// Get the table schema that is presumably shared across agents.
		mdsSchemaReq := &metadatapb.SchemaRequest{}
		mdsSchemaResp, err := a.mdsClient.GetSchemas(ctx, mdsSchemaReq)
		if err != nil {
			handleError(err)
			return
		}
		schema = mdsSchemaResp.Schema
	}

	agentsInfo, err := NewAgentsInfo(schema, agentInfosResp, mdsAgentTableMetadataResp)
	if err != nil {
		handleError(err)
		return
	}

	a.agentsInfoMu.Lock()
	defer a.agentsInfoMu.Unlock()
	a.agentsInfo = agentsInfo
}
