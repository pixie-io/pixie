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
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/carnot/carnotpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/carnot/queryresultspb"
	"px.dev/pixie/src/utils"
)

var queryExecRecordsSummary *prometheus.SummaryVec
var queryExecBytesSummary *prometheus.SummaryVec

func init() {
	queryExecRecordsSummary = promauto.NewSummaryVec(
		prometheus.SummaryOpts{
			Name: "query_exec_records_processed",
			Help: "A summary of the number of records processed running the given script.",
		},
		[]string{"script_name"},
	)
	queryExecBytesSummary = promauto.NewSummaryVec(
		prometheus.SummaryOpts{
			Name: "query_exec_bytes_processed",
			Help: "A summary of the number of bytes processed running the given script.",
		},
		[]string{"script_name"},
	)
}

// QueryPlanOpts contains options for generating and returning the query plan
// when the query has explain=true.
type QueryPlanOpts struct {
	TableID string
	Plan    *distributedpb.DistributedPlan
	PlanMap map[uuid.UUID]*planpb.Plan
}

// The deadline for all sinks in a given query to initialize.
const defaultResultSinkInitializationTimeout = 30 * time.Second

// The size of each activeQuery's buffered result channel.
const activeQueryBufferSize = 1024

// The timeout for not receiving any data from producers.
const defaultProducerTimeout = 180 * time.Second

// The timeout for not sending any data to consumers.
// Consumer timeout must be greater than or equal to the producer timeout
// since the consumer can't consume data if the producers aren't sending any data.
const defaultConsumerTimeout = 180 * time.Second

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

	// Store this info so that resumed queries can return compilation time and optionally query plans.
	compilationTimeNs int64
	queryPlanOpts     *QueryPlanOpts

	// Consumers must register their context cancel funcs, so that they can be cancelled by the watchdog.
	registerConsumerCh chan context.CancelFunc
	// Consumers and producers must send periodic health checks to these channels so that the watchdog can end the query if consumers or producers die.
	consumerHealthcheckCh chan bool
	producerHealthcheckCh chan bool
	// We store a cancel func for the watchdogs context so that the query as a whole can be cancelled.
	cancelQueryFunc  context.CancelFunc
	cancelQueryError error

	// We store a single producer context that all producers can access, so that we can cancel all consumers at once.
	producerCtx context.Context

	// Name used for labeling metrics recorded for this query.
	queryName string
}

func newActiveQuery(producerCtx context.Context, tableIDMap map[string]string,
	compilationTimeNs int64,
	queryPlanOpts *QueryPlanOpts, watchdogCancel context.CancelFunc, queryName string) *activeQuery {
	aq := &activeQuery{
		queryResultCh: make(chan *carnotpb.TransferResultChunkRequest, activeQueryBufferSize),
		tableIDMap:    tableIDMap,

		uninitializedTables: &concurrentSet{unsafeMap: make(map[string]struct{})},
		remainingTableEos:   &concurrentSet{unsafeMap: make(map[string]struct{})},

		allTablesConnectedCh: make(chan struct{}),

		gotFinalExecStats: false,
		// Store compilation time and query plan opts so that callers of ResumeQuery don't need to be aware of these.
		compilationTimeNs: compilationTimeNs,
		queryPlanOpts:     queryPlanOpts,

		registerConsumerCh:    make(chan context.CancelFunc),
		consumerHealthcheckCh: make(chan bool),
		producerHealthcheckCh: make(chan bool),

		cancelQueryFunc: watchdogCancel,
		producerCtx:     producerCtx,

		queryName: queryName,
	}

	for tableName := range tableIDMap {
		aq.remainingTableEos.add(tableName)
		aq.uninitializedTables.add(tableName)
	}

	return aq
}

// This function and queryComplete() should only be called by the same single thread.
func (a *activeQuery) updateQueryState(msg *carnotpb.TransferResultChunkRequest) error {
	queryIDStr := utils.UUIDFromProtoOrNil(msg.QueryID).String()

	if initConn := msg.GetInitiateConn(); initConn != nil {
		return nil
	}

	// Mark down that we received the exec stats for this query.
	if execStats := msg.GetExecutionAndTimingInfo(); execStats != nil {
		if a.gotFinalExecStats {
			return fmt.Errorf("already received exec stats for query %s", queryIDStr)
		}
		a.gotFinalExecStats = true
		if stats := execStats.GetExecutionStats(); stats != nil {
			queryExecRecordsSummary.With(prometheus.Labels{"script_name": a.queryName}).Observe(float64(stats.RecordsProcessed))
			queryExecBytesSummary.With(prometheus.Labels{"script_name": a.queryName}).Observe(float64(stats.BytesProcessed))
		}
		return nil
	}

	// Update the set of tables we are waiting on EOS from.
	if queryResult := msg.GetQueryResult(); queryResult != nil {
		tableName := queryResult.GetTableName()

		if rb := queryResult.GetRowBatch(); rb != nil {
			if a.uninitializedTables.exists(tableName) {
				a.uninitializedTables.remove(tableName)
				if a.uninitializedTables.size() == 0 {
					close(a.allTablesConnectedCh)
				}
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
	}

	// The error is handled in another part of the code. We do nothing here.
	if execError := msg.GetExecutionError(); execError != nil {
		return nil
	}
	return fmt.Errorf("error in ForwardQueryResult: Expected TransferResultChunkRequest to have init message, row batch, exec stats, exec error")
}

func (a *activeQuery) queryComplete() bool {
	return a.uninitializedTables.size() == 0 && a.remainingTableEos.size() == 0 && a.gotFinalExecStats
}

func (a *activeQuery) registerConsumer(cancel context.CancelFunc) error {
	// If the watchdog has already exited, then the write to the channel will hang, so we have a timeout.
	t := time.NewTimer(time.Second)
	select {
	case a.registerConsumerCh <- cancel:
	case <-t.C:
		return fmt.Errorf("timedout trying to register consumer")
	}
	return nil
}

func (a *activeQuery) watchdog(ctx context.Context, queryID uuid.UUID, consumerTimeout time.Duration, producerTimeout time.Duration, deleteQuery func()) {
	consumerTimer := time.NewTimer(consumerTimeout)
	producerTimer := time.NewTimer(producerTimeout)
	resetTimer := func(t *time.Timer, timeout time.Duration) {
		if !t.Stop() {
			<-t.C
		}
		t.Reset(timeout)
	}
	cancelConsumer := func() {}
forLoop:
	for {
		select {
		case cancel := <-a.registerConsumerCh:
			// We only allow one consumer so cancel the old consumer if there was one.
			cancelConsumer()
			cancelConsumer = cancel
			resetTimer(consumerTimer, consumerTimeout)

		case <-a.consumerHealthcheckCh:
			resetTimer(consumerTimer, consumerTimeout)
		case <-a.producerHealthcheckCh:
			resetTimer(producerTimer, producerTimeout)

		case <-consumerTimer.C:
			a.cancelQueryError = fmt.Errorf("Query %s timedout waiting for consumer", queryID.String())
			break forLoop
		case <-producerTimer.C:
			a.cancelQueryError = fmt.Errorf("Query %s timedout waiting for producers", queryID.String())
			break forLoop

		case <-ctx.Done():
			break forLoop
		}
	}

	// Shutdown and delete query
	deleteQuery()
	cancelConsumer()
}

func (a *activeQuery) handleRequest(ctx context.Context, queryID uuid.UUID, msg *carnotpb.TransferResultChunkRequest, resultCh chan<- *vizierpb.ExecuteScriptResponse) error {
	// Stream the agent stream result to the client stream.
	// Check if stream is complete. If so, close client stream.
	// If there was an error, then cancel both sides of the stream.
	err := a.updateQueryState(msg)
	if err != nil {
		return err
	}

	// Optionally send the query plan (which requires the exec stats).
	if execStats := msg.GetExecutionAndTimingInfo(); execStats != nil {
		a.agentExecStats = &(execStats.AgentExecutionStats)
	}

	// If the query is complete and we need to send the query plan, send it before the final
	// execution stats, since consumers may expect those to be the last message.
	if a.queryComplete() {
		if a.queryPlanOpts != nil {
			qpResps, err := QueryPlanResponse(queryID, a.queryPlanOpts.Plan, a.queryPlanOpts.PlanMap,
				a.agentExecStats, a.queryPlanOpts.TableID, maxQueryPlanStringSize)

			if err != nil {
				return err
			}
			for _, qpRes := range qpResps {
				select {
				case <-ctx.Done():
					return nil
				case resultCh <- qpRes:
				}
			}
		}
	}

	resp, err := BuildExecuteScriptResponse(msg, a.tableIDMap, a.compilationTimeNs)
	if err != nil {
		return err
	}

	// Some inbound messages don't translate into responses to the client stream.
	if resp == nil {
		return nil
	}

	select {
	case <-ctx.Done():
		return nil
	case resultCh <- resp:
	}
	if status := resp.GetStatus(); status != nil && status.Code != 0 {
		return VizierStatusToError(status)
	}

	return nil
}

func (a *activeQuery) consumerHealthcheck(ctx context.Context) {
	select {
	case <-ctx.Done():
	case a.consumerHealthcheckCh <- true:
	}
}

func (a *activeQuery) producerHealthcheck(ctx context.Context) {
	select {
	case <-ctx.Done():
		// producerCtx might complete after an activeQuery is already in use by a producer.
		// The producer, not knowing that yet, will call producerHealthcheck() which would try to
		// write to producerHealthcheckCh. The reader for producerHealthcheckCh will be dead so we would
	// just hang.
	case <-a.producerCtx.Done():
	case a.producerHealthcheckCh <- true:
	}
}

func (a *activeQuery) cancelQuery(err error) {
	a.cancelQueryError = err
	a.cancelQueryFunc()
}

// QueryResultForwarder is responsible for receiving query results from the agent streams and forwarding
// that data to the client stream.
type QueryResultForwarder interface {
	RegisterQuery(queryID uuid.UUID, tableIDMap map[string]string,
		compilationTimeNs int64,
		queryPlanOpts *QueryPlanOpts, queryName string) error

	// Streams results from the agent stream to the client stream.
	// Blocks until the stream (& the agent stream) has completed, been cancelled, or experienced an error.
	// Returns error for any error received.
	StreamResults(ctx context.Context, queryID uuid.UUID,
		resultCh chan<- *vizierpb.ExecuteScriptResponse) error

	// Returns the producer context for the query, so that the watchdog can cancel all producers with one context.
	GetProducerCtx(queryID uuid.UUID) (context.Context, error)
	// Pass a message received from the agent stream to the client-side stream.
	ForwardQueryResult(ctx context.Context, msg *carnotpb.TransferResultChunkRequest) error
	// If the producer of data (i.e. Kelvin) errors then this function can be used to shutdown the stream.
	// The producer should not call this function if there's a retry is possible.
	ProducerCancelStream(queryID uuid.UUID, err error)
}

// QueryResultForwarderImpl implements the QueryResultForwarder interface.
type QueryResultForwarderImpl struct {
	activeQueries map[uuid.UUID]*activeQuery
	// Used to guard deletions and accesses of the activeQueries map.
	activeQueriesMutex              sync.Mutex
	resultSinkInitializationTimeout time.Duration

	consumerTimeout time.Duration
	producerTimeout time.Duration
}

// QueryResultForwarderOption allows specifying options for new QueryResultForwarders.
type QueryResultForwarderOption func(*QueryResultForwarderImpl)

// WithResultSinkTimeout sets the result sink initialization timeout.
func WithResultSinkTimeout(timeout time.Duration) QueryResultForwarderOption {
	return func(rf *QueryResultForwarderImpl) {
		rf.resultSinkInitializationTimeout = timeout
	}
}

// WithProducerTimeout sets a timeout on how long the result forwarder will wait without seeing any data from producers.
func WithProducerTimeout(timeout time.Duration) QueryResultForwarderOption {
	return func(rf *QueryResultForwarderImpl) {
		rf.producerTimeout = timeout
	}
}

// WithConsumerTimeout sets a timeout on how long the result forwarder will wait without a consumer pulling data from it.
func WithConsumerTimeout(timeout time.Duration) QueryResultForwarderOption {
	return func(rf *QueryResultForwarderImpl) {
		rf.consumerTimeout = timeout
	}
}

// NewQueryResultForwarder creates a new QueryResultForwarder.
func NewQueryResultForwarder() QueryResultForwarder {
	return NewQueryResultForwarderWithOptions()
}

// NewQueryResultForwarderWithOptions returns a query result forwarder with custom options.
func NewQueryResultForwarderWithOptions(opts ...QueryResultForwarderOption) QueryResultForwarder {
	rf := &QueryResultForwarderImpl{
		activeQueries:                   make(map[uuid.UUID]*activeQuery),
		resultSinkInitializationTimeout: defaultResultSinkInitializationTimeout,

		consumerTimeout: defaultConsumerTimeout,
		producerTimeout: defaultProducerTimeout,
	}

	for _, opt := range opts {
		opt(rf)
	}
	return rf
}

// RegisterQuery registers a query ID in the result forwarder.
func (f *QueryResultForwarderImpl) RegisterQuery(queryID uuid.UUID, tableIDMap map[string]string,
	compilationTimeNs int64,
	queryPlanOpts *QueryPlanOpts,
	queryName string) error {
	f.activeQueriesMutex.Lock()
	defer f.activeQueriesMutex.Unlock()

	if _, present := f.activeQueries[queryID]; present {
		return fmt.Errorf("Query %d already registered", queryID)
	}
	watchdogCtx, watchdogCancel := context.WithCancel(context.Background())
	producerCtx, producerCancel := context.WithCancel(context.Background())
	aq := newActiveQuery(producerCtx, tableIDMap, compilationTimeNs, queryPlanOpts, watchdogCancel, queryName)
	f.activeQueries[queryID] = aq

	deleteQuery := func() {
		f.activeQueriesMutex.Lock()
		defer f.activeQueriesMutex.Unlock()
		delete(f.activeQueries, queryID)
		producerCancel()
	}
	go aq.watchdog(watchdogCtx, queryID, f.consumerTimeout, f.producerTimeout, deleteQuery)
	return nil
}

// The max size of the query plan string, including a buffer for the rest of the message.
const maxQueryPlanBufferSize int = 64 * 1024
const maxQueryPlanStringSize = 1024*1024 - maxQueryPlanBufferSize

// StreamResults streams results from the agent streams to the client stream.
func (f *QueryResultForwarderImpl) StreamResults(ctx context.Context, queryID uuid.UUID,
	resultCh chan<- *vizierpb.ExecuteScriptResponse) error {
	f.activeQueriesMutex.Lock()
	activeQuery, present := f.activeQueries[queryID]
	f.activeQueriesMutex.Unlock()

	if !present {
		return fmt.Errorf("error in StreamResults: Query %s not registered in query forwarder", queryID.String())
	}

	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	if err := activeQuery.registerConsumer(cancel); err != nil {
		return err
	}

	// Waits for `resultSinkInitializationTimeout` time for all of the result sinks (tables)
	// for this query to initialize a connection to the query broker.
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case <-activeQuery.allTablesConnectedCh:
				return
			case <-time.After(f.resultSinkInitializationTimeout):
				missingSinks := activeQuery.uninitializedTables.values()
				err := fmt.Errorf("Query %s failed to initialize all result tables within the deadline, missing: %s",
					queryID.String(), strings.Join(missingSinks, ", "))
				log.Info(err.Error())
				activeQuery.cancelQuery(err)
				return
			}
		}
	}()

	for {
		select {
		case <-ctx.Done():
			return activeQuery.cancelQueryError

		case msg := <-activeQuery.queryResultCh:
			activeQuery.consumerHealthcheck(ctx)
			if err := activeQuery.handleRequest(ctx, queryID, msg, resultCh); err != nil {
				activeQuery.cancelQuery(err)
				return err
			}
			if activeQuery.queryComplete() {
				activeQuery.cancelQuery(nil)
				return nil
			}
		}
	}
}

// GetProducerCtx returns the producer context for the query, so producers can check for that context being cancelled.
func (f *QueryResultForwarderImpl) GetProducerCtx(queryID uuid.UUID) (context.Context, error) {
	f.activeQueriesMutex.Lock()
	activeQuery, present := f.activeQueries[queryID]
	f.activeQueriesMutex.Unlock()

	if !present {
		return nil, fmt.Errorf("error in GetProducerCtx: Query %s is not registered in query forwarder", queryID.String())
	}

	return activeQuery.producerCtx, nil
}

// ForwardQueryResult forwards the agent result to the client result channel.
func (f *QueryResultForwarderImpl) ForwardQueryResult(ctx context.Context, msg *carnotpb.TransferResultChunkRequest) error {
	queryID := utils.UUIDFromProtoOrNil(msg.QueryID)
	f.activeQueriesMutex.Lock()
	activeQuery, present := f.activeQueries[queryID]
	f.activeQueriesMutex.Unlock()

	if !present {
		return fmt.Errorf("error in ForwardQueryResult: Query %s is not registered in query forwarder", queryID.String())
	}

	select {
	case <-activeQuery.producerCtx.Done():
		return activeQuery.cancelQueryError
	case <-ctx.Done():
		return activeQuery.cancelQueryError
	case activeQuery.queryResultCh <- msg:
		activeQuery.producerHealthcheck(ctx)
		return nil
	default:
		err := fmt.Errorf("error in ForwardQueryResult: Query %s can not accept more data, consumer likely died", queryID.String())
		activeQuery.cancelQuery(err)
		return err
	}
}

// ProducerCancelStream signals to StreamResults that the client stream should be
// cancelled. It is triggered by the handler for the agent streams.
func (f *QueryResultForwarderImpl) ProducerCancelStream(queryID uuid.UUID, err error) {
	f.activeQueriesMutex.Lock()
	activeQuery, present := f.activeQueries[queryID]
	f.activeQueriesMutex.Unlock()
	// It's ok to cancel a query that doesn't currently exist in the system, since it may have already
	// been cleaned up.
	if !present {
		return
	}
	// Cancel the query if it hasn't already been cancelled.
	activeQuery.cancelQuery(err)
}
