package controllers

import (
	"context"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"

	public_vizierapipb "px.dev/pixie/src/api/public/vizierapipb"
	"px.dev/pixie/src/carnot/carnotpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/carnot/queryresultspb"
	"px.dev/pixie/src/utils"
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

var void = struct{}{}

type concurrentSet struct {
	unsafeMap map[string]struct{}
	mu        sync.RWMutex
}

func (s *concurrentSet) add(key string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.unsafeMap[key] = void
}

func (s *concurrentSet) exists(key string) bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	_, exists := s.unsafeMap[key]
	return exists
}

func (s *concurrentSet) remove(key string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.unsafeMap, key)
}

func (s *concurrentSet) values() []string {
	s.mu.RLock()
	defer s.mu.RUnlock()

	vals := make([]string, len(s.unsafeMap))
	i := 0
	for k := range s.unsafeMap {
		vals[i] = k
		i++
	}
	return vals
}

func (s *concurrentSet) size() int {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return len(s.unsafeMap)
}

// A struct to track state for an active query in the system.
// It can be modified and accessed by multiple agent streams and a single client stream.
type activeQuery struct {
	// Signal to cancel the client stream for this query.
	cancelClientStreamCh    chan struct{}
	cancelClientStreamError error
	cancelOnce              sync.Once

	queryResultCh chan *carnotpb.TransferResultChunkRequest
	tableIDMap    map[string]string

	// The set of result tables that still need to open a connection to this
	// result forwarder via TransferResultChunk. If they take too long to open the
	// connection, the query will time out.
	uninitializedTables *concurrentSet

	// Signal for when all of the expected tables have established a connection.
	allTablesConnectedCh chan struct{}

	// The tables left in the query for which to receive end of stream.
	// These are deleted as end of stream signals come in.
	// These two fields are only accessed by a single writer and reader.
	remainingTableEos *concurrentSet

	gotFinalExecStats bool
	agentExecStats    *[]*queryresultspb.AgentExecutionStats
}

func newActiveQuery(tableIDMap map[string]string) *activeQuery {
	aq := &activeQuery{
		cancelClientStreamCh: make(chan struct{}),

		queryResultCh: make(chan *carnotpb.TransferResultChunkRequest),
		tableIDMap:    tableIDMap,

		uninitializedTables: &concurrentSet{unsafeMap: make(map[string]struct{})},
		remainingTableEos:   &concurrentSet{unsafeMap: make(map[string]struct{})},

		allTablesConnectedCh: make(chan struct{}),

		gotFinalExecStats: false,
	}

	for tableName := range tableIDMap {
		aq.remainingTableEos.add(tableName)
		aq.uninitializedTables.add(tableName)
	}

	return aq
}

func (a *activeQuery) signalCancelClientStream(err error) {
	a.cancelOnce.Do(func() {
		a.cancelClientStreamError = err
		close(a.cancelClientStreamCh)
	})
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
			if a.uninitializedTables.exists(tableName) {
				return fmt.Errorf("Received RowBatch before initializing table %s for query %s",
					tableName, queryIDStr)
			}

			if !rb.GetEos() {
				return nil
			}

			if !a.remainingTableEos.exists(tableName) {
				return fmt.Errorf("received multiple EOS for table name '%s' for query ID %s", tableName, queryIDStr)
			}
			a.remainingTableEos.remove(tableName)
			return nil
		}

		if queryResult.GetInitiateResultStream() {
			if !a.uninitializedTables.exists(tableName) {
				return fmt.Errorf("Did not expect stream to be (re)opened query result table %s for query %s",
					tableName, queryIDStr)
			}
			a.uninitializedTables.remove(tableName)
			// If we have initialized all of our tables, then signal to the goroutine waiting for all
			// result sinks to be initialized that it can stop waiting.
			if a.uninitializedTables.size() == 0 {
				close(a.allTablesConnectedCh)
			}
			return nil
		}
	}

	return fmt.Errorf("error in ForwardQueryResult: Expected TransferResultChunkRequest to have query result or exec stats")
}

func (a *activeQuery) queryComplete() bool {
	return a.uninitializedTables.size() == 0 && a.remainingTableEos.size() == 0 && a.gotFinalExecStats
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
		resultCh chan *public_vizierapipb.ExecuteScriptResponse,
		compilationTimeNs int64,
		queryPlanOpts *QueryPlanOpts) error

	// Pass a message received from the agent stream to the client-side stream.
	ForwardQueryResult(msg *carnotpb.TransferResultChunkRequest) error
	// Send a signal to cancel the query (both sides of the stream should be cancelled).
	// It is safe to call this function multiple times.
	OptionallyCancelClientStream(queryID uuid.UUID, err error)
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
	return nil
}

// DeleteQuery deletes a query ID in the result forwarder.
func (f *QueryResultForwarderImpl) DeleteQuery(queryID uuid.UUID) {
	f.activeQueriesMutex.Lock()
	defer f.activeQueriesMutex.Unlock()
	delete(f.activeQueries, queryID)
}

// The max size of the query plan string, including a buffer for the rest of the message.
const maxQueryPlanBufferSize int = 64 * 1024
const maxQueryPlanStringSize = 1024*1024 - maxQueryPlanBufferSize

// StreamResults streams results from the agent streams to the client stream.
func (f *QueryResultForwarderImpl) StreamResults(ctx context.Context, queryID uuid.UUID,
	resultCh chan *public_vizierapipb.ExecuteScriptResponse,
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
		activeQuery.signalCancelClientStream(err)
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
				missingSinks := activeQuery.uninitializedTables.values()
				err := fmt.Errorf("Query %s failed to initialize all result tables within the deadline, missing: %s",
					queryID.String(), strings.Join(missingSinks, ", "))
				log.Info(err.Error())
				activeQuery.signalCancelClientStream(err)
				return
			}
		}
	}()

	for {
		select {
		case <-ctx.Done():
			// Client side stream is cancelled.
			// Subsequent calls to ForwardQueryResult should fail for this query.
			activeQuery.signalCancelClientStream(nil)
			return nil

		case <-activeQuery.cancelClientStreamCh:
			return activeQuery.cancelClientStreamError

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
					qpResps, err := QueryPlanResponse(queryID, queryPlanOpts.Plan, queryPlanOpts.PlanMap,
						activeQuery.agentExecStats, queryPlanOpts.TableID, maxQueryPlanStringSize)

					if err != nil {
						return cancelStreamReturnErr(err)
					}
					for _, qpRes := range qpResps {
						resultCh <- qpRes
					}
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
		return fmt.Errorf("query result not forwarded, query %s has been cancelled", queryID.String())
	}
}

// OptionallyCancelClientStream signals to StreamResults that the client stream should be
// cancelled. It is triggered by the handler for the agent streams.
func (f *QueryResultForwarderImpl) OptionallyCancelClientStream(queryID uuid.UUID, err error) {
	f.activeQueriesMutex.Lock()
	activeQuery, present := f.activeQueries[queryID]
	f.activeQueriesMutex.Unlock()
	// It's ok to cancel a query that doesn't currently exist in the system, since it may have already
	// been cleaned up.
	if !present {
		return
	}
	// Cancel the client stream if it hasn't already been cancelled.
	activeQuery.signalCancelClientStream(err)
}
