package controllers

import (
	"context"
	"fmt"
	"strings"
	"sync"
	"time"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/carnotpb"
	"pixielabs.ai/pixielabs/src/utils"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

// QueryPlanOpts contains options for generating and returning the query plan
// when the query has explain=true.
type QueryPlanOpts struct {
	TableID string
	Plan    *distributedpb.DistributedPlan
	PlanMap map[uuid.UUID]*planpb.Plan
}

// The deadline for all sinks in a given query to initialize.
const defaultResultSinkInitializationTimeout = 5 * time.Second

// A struct to track state for an active query in the system.
// It can be modified and accessed by multiple agent streams and a single client stream.
type activeQuery struct {
	// Signal to cancel the client stream for this query.
	cancelClientStreamCh chan bool
	// There are multiple agent streams per query, any of which can trigger cancellation
	// in the client stream. These fields ensure that multiple agent streams calling
	// cancel on the agent stream (aka closing the cancelClientStreamCh) is safe.
	cancelClientStreamMutex sync.Mutex
	clientStreamCancelled   bool

	queryResultCh chan *carnotpb.TransferResultChunkRequest
	tableIDMap    map[string]string

	// The set of result tables that still need to open a connection to this
	// result forwarder via TransferResultChunk. If they take too long to open the
	// connection, the query will time out.
	uninitializedTables map[string]bool
	// Signal for when all of the expected tables have established a connection.
	allTablesConnectedCh chan bool

	// The tables left in the query for which to receive end of stream.
	// These are deleted as end of stream signals come in.
	// These two fields are only accessed by a single writer and reader.
	remainingTableEos map[string]bool
	gotFinalExecStats bool
	agentExecStats    *[]*queryresultspb.AgentExecutionStats
}

func newActiveQuery(tableIDMap map[string]string) *activeQuery {
	eosTables := make(map[string]bool)
	uninitializedTables := make(map[string]bool)
	for tableName := range tableIDMap {
		eosTables[tableName] = true
		uninitializedTables[tableName] = true
	}

	return &activeQuery{
		queryResultCh:         make(chan *carnotpb.TransferResultChunkRequest),
		cancelClientStreamCh:  make(chan bool),
		clientStreamCancelled: false,
		remainingTableEos:     eosTables,
		uninitializedTables:   uninitializedTables,
		gotFinalExecStats:     false,
		tableIDMap:            tableIDMap,
		allTablesConnectedCh:  make(chan bool),
	}
}

func (a *activeQuery) signalCancelClientStream() {
	a.cancelClientStreamMutex.Lock()
	defer a.cancelClientStreamMutex.Unlock()

	// Cancel the client stream if it hasn't already been cancelled.
	if a.clientStreamCancelled {
		// Another agent stream has already cancelled this.
		return
	}
	a.clientStreamCancelled = true
	close(a.cancelClientStreamCh)
}

// This function and queryComplete() should only be called by the same single thread.
func (a *activeQuery) updateQueryState(msg *carnotpb.TransferResultChunkRequest) error {
	queryIDStr := utils.UUIDFromProtoOrNil(msg.QueryID).String()

	// Mark down that we received the exec stats for this query.
	if execStats := msg.GetExecutionAndTimingInfo(); execStats != nil {
		if a.gotFinalExecStats {
			return fmt.Errorf("already received exec stats for query %s", queryIDStr)
		}
		a.gotFinalExecStats = true
		return nil
	}

	// Update the set of tables we are waiting on EOS from.
	if queryResult := msg.GetQueryResult(); queryResult != nil {
		tableName := queryResult.GetTableName()

		if rb := queryResult.GetRowBatch(); rb != nil {
			if _, present := a.uninitializedTables[tableName]; present {
				return fmt.Errorf("Received RowBatch before initializing table %s for query %s",
					tableName, queryIDStr)
			}

			if !rb.GetEos() {
				return nil
			}

			if _, present := a.remainingTableEos[tableName]; present {
				delete(a.remainingTableEos, tableName)
				return nil
			}
			return fmt.Errorf("unexpected table name '%s' for query ID %s", tableName, queryIDStr)
		}

		if queryResult.GetInitiateResultStream() {
			if _, present := a.uninitializedTables[tableName]; !present {
				return fmt.Errorf("Did not expect stream to be (re)opened query result table %s for query %s",
					tableName, queryIDStr)
			}
			delete(a.uninitializedTables, tableName)
			// If we have initialized all of our tables, then signal to the goroutine waiting for all
			// result sinks to be initialized that it can stop waiting.
			if len(a.uninitializedTables) == 0 {
				close(a.allTablesConnectedCh)
			}
			return nil
		}
	}

	return fmt.Errorf("error in ForwardQueryResult: Expected TransferResultChunkRequest to have query result or exec stats")
}

func (a *activeQuery) queryComplete() bool {
	return len(a.uninitializedTables) == 0 && len(a.remainingTableEos) == 0 && a.gotFinalExecStats
}

// QueryResultForwarder is responsible for receiving query results from the agent streams and forwarding
// that data to the client stream.
type QueryResultForwarder interface {
	RegisterQuery(queryID uuid.UUID, tableIDMap map[string]string) error
	// To be used if a query needs to be deleted before StreamResults is invoked.
	// Otherwise, StreamResults will delete the query for the caller.
	DeleteQuery(queryID uuid.UUID)

	// Streams results from the agent stream to the client stream.
	// Blocks until the stream (& the agent stream) has completed, been cancelled, or experienced an error.
	// Returns error for any error received.
	StreamResults(ctx context.Context, queryID uuid.UUID,
		resultCh chan *vizierpb.ExecuteScriptResponse,
		compilationTimeNs int64,
		queryPlanOpts *QueryPlanOpts) error

	// Pass a message received from the agent stream to the client-side stream.
	ForwardQueryResult(msg *carnotpb.TransferResultChunkRequest) error
	// Send a signal to cancel the query (both sides of the stream should be cancelled).
	// It is safe to call this function multiple times.
	OptionallyCancelClientStream(queryID uuid.UUID)
}

// QueryResultForwarderImpl implements the QueryResultForwarder interface.
type QueryResultForwarderImpl struct {
	activeQueries map[uuid.UUID]*activeQuery
	// Used to guard deletions and accesses of the activeQueries map.
	activeQueriesMutex              sync.Mutex
	resultSinkInitializationTimeout time.Duration
}

// NewQueryResultForwarder creates a new QueryResultForwarder.
func NewQueryResultForwarder() QueryResultForwarder {
	return NewQueryResultForwarderWithTimeout(defaultResultSinkInitializationTimeout)
}

// NewQueryResultForwarderWithTimeout returns a query result forwarder with a custom timeout.
func NewQueryResultForwarderWithTimeout(timeout time.Duration) QueryResultForwarder {
	return &QueryResultForwarderImpl{
		activeQueries:                   make(map[uuid.UUID]*activeQuery),
		resultSinkInitializationTimeout: timeout,
	}
}

// RegisterQuery registers a query ID in the result forwarder.
func (f *QueryResultForwarderImpl) RegisterQuery(queryID uuid.UUID, tableIDMap map[string]string) error {
	f.activeQueriesMutex.Lock()
	defer f.activeQueriesMutex.Unlock()

	if _, present := f.activeQueries[queryID]; present {
		return fmt.Errorf("Query %d already registered", queryID)
	}
	f.activeQueries[queryID] = newActiveQuery(tableIDMap)
	log.Errorf("Registering query %s", queryID.String())
	return nil
}

// DeleteQuery deletes a query ID in the result forwarder.
func (f *QueryResultForwarderImpl) DeleteQuery(queryID uuid.UUID) {
	f.activeQueriesMutex.Lock()
	defer f.activeQueriesMutex.Unlock()
	delete(f.activeQueries, queryID)
}

// StreamResults streams results from the agent streams to the client stream.
func (f *QueryResultForwarderImpl) StreamResults(ctx context.Context, queryID uuid.UUID,
	resultCh chan *vizierpb.ExecuteScriptResponse,
	compilationTimeNs int64, queryPlanOpts *QueryPlanOpts) error {

	f.activeQueriesMutex.Lock()
	activeQuery, present := f.activeQueries[queryID]
	f.activeQueriesMutex.Unlock()

	if !present {
		return fmt.Errorf("error in StreamResults: Query %s not registered in query forwarder", queryID.String())
	}

	defer func() {
		f.activeQueriesMutex.Lock()
		delete(f.activeQueries, queryID)
		f.activeQueriesMutex.Unlock()
	}()

	ctx, cancel := context.WithCancel(ctx)
	cancelStreamReturnErr := func(err error) error {
		activeQuery.signalCancelClientStream()
		cancel()
		return err
	}

	// Waits for `resultSinkInitializationTimeout` time for all of the result sinks (tables)
	// for this query to initialize a connection to the query broker.
	go func() {
		for {
			select {
			case <-activeQuery.cancelClientStreamCh:
				return
			case <-activeQuery.allTablesConnectedCh:
				return
			case <-time.After(f.resultSinkInitializationTimeout):
				var missingSinks []string
				for table := range activeQuery.uninitializedTables {
					missingSinks = append(missingSinks, table)
				}
				log.Errorf("Query %s failed to initialize all result tables within the deadline, missing: %s",
					queryID.String(), strings.Join(missingSinks, ", "))
				activeQuery.signalCancelClientStream()
				return
			}
		}
	}()

	for {
		select {
		case <-ctx.Done():
			// Client side stream is cancelled.
			// Subsequent calls to ForwardQueryResult should fail for this query.
			activeQuery.signalCancelClientStream()
			return nil

		case <-activeQuery.cancelClientStreamCh:
			return cancelStreamReturnErr(
				fmt.Errorf("Client stream cancelled for query %s", queryID.String()))

		case msg := <-activeQuery.queryResultCh:
			// Stream the agent stream result to the client stream.
			// Check if stream is complete. If so, close client stream.
			// If there was an error, then cancel both sides of the stream.
			err := activeQuery.updateQueryState(msg)
			if err != nil {
				return cancelStreamReturnErr(err)
			}

			// Optionally send the query plan (which requires the exec stats).
			if execStats := msg.GetExecutionAndTimingInfo(); execStats != nil {
				activeQuery.agentExecStats = &(execStats.AgentExecutionStats)
			}

			// If the query is complete and we need to send the query plan, send it before the final
			// execution stats, since consumers may expect those to be the last message.
			if activeQuery.queryComplete() {
				if queryPlanOpts != nil {
					qpRes, err := QueryPlanResponse(queryID, queryPlanOpts.Plan, queryPlanOpts.PlanMap,
						activeQuery.agentExecStats, queryPlanOpts.TableID)

					if err != nil {
						return cancelStreamReturnErr(err)
					}
					resultCh <- qpRes
				}
			}

			resp, err := BuildExecuteScriptResponse(msg, activeQuery.tableIDMap, compilationTimeNs)
			if err != nil {
				return cancelStreamReturnErr(err)
			}

			// Some inbound messages don't translate into responses to the client stream.
			if resp != nil {
				resultCh <- resp
			}

			if activeQuery.queryComplete() {
				return nil
			}
		}
	}
}

// ForwardQueryResult forwards the agent result to the client result channel.
func (f *QueryResultForwarderImpl) ForwardQueryResult(msg *carnotpb.TransferResultChunkRequest) error {
	queryID := utils.UUIDFromProtoOrNil(msg.QueryID)
	f.activeQueriesMutex.Lock()
	activeQuery, present := f.activeQueries[queryID]
	f.activeQueriesMutex.Unlock()

	// It's ok to cancel a query that doesn't currently exist in the system, since it may have already
	// been cleaned up.
	if !present {
		return fmt.Errorf("error in ForwardQueryResult: Query %s is not registered in query forwarder", queryID.String())
	}

	select {
	case activeQuery.queryResultCh <- msg:
		return nil
	case <-activeQuery.cancelClientStreamCh:
		return fmt.Errorf("query result not forwarded, query %d has been cancelled", queryID.String())
	}
}

// OptionallyCancelClientStream signals to StreamResults that the client stream should be
// cancelled. It is triggered by the handler for the agent streams.
func (f *QueryResultForwarderImpl) OptionallyCancelClientStream(queryID uuid.UUID) {
	f.activeQueriesMutex.Lock()
	activeQuery, present := f.activeQueries[queryID]
	f.activeQueriesMutex.Unlock()
	// It's ok to cancel a query that doesn't currently exist in the system, since it may have already
	// been cleaned up.
	if !present {
		return
	}
	// Cancel the client stream if it hasn't already been cancelled.
	activeQuery.signalCancelClientStream()
}
