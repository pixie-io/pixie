package controllers

import (
	"fmt"

	uuid "github.com/satori/go.uuid"

	"pixielabs.ai/pixielabs/src/carnotpb"
	"pixielabs.ai/pixielabs/src/utils"
)

// QueryResultForwarder is responsible for receiving query results from agents
// and forwarding that data to the client stream. It also needs to make sure that
// streams on one side get closed if the other side stream gets cancelled.
type QueryResultForwarder interface {
	// Pass a message received from an agent to the client-side stream.
	ForwardQueryResult(msg *carnotpb.TransferResultChunkRequest) error
	// Close client-side stream for the given queryID, if it still exists.
	// Will not return an error if the stream does not exist in the result forwarder,
	// since it may have already been cancelled previously.
	OptionallyCancelClientStream(queryID uuid.UUID) error
	// TODO(nserrino): Add methods to register a query ID from the client-side stream.
}

// QueryResultForwarderImpl implements the QueryResultForwarder interface.
type QueryResultForwarderImpl struct {
	// TODO(nserrino): Remove this reference when the batch API ReceiveAgentQueryResult
	// is deprecated in Kelvin and the query broker. QueryResultForwarderImpl will deal with
	// the client streams directly, rather than the Executors.
	executors map[uuid.UUID]Executor
}

// NewQueryResultForwarder creates a new QueryResultForwarder.
func NewQueryResultForwarder(queryExecutors map[uuid.UUID]Executor) QueryResultForwarder {
	return &QueryResultForwarderImpl{
		executors: queryExecutors,
	}
}

// ForwardQueryResult forwards the agent result to the client result stream.
func (f *QueryResultForwarderImpl) ForwardQueryResult(msg *carnotpb.TransferResultChunkRequest) error {
	msgQueryID := utils.UUIDFromProtoOrNil(msg.QueryID)
	if executor, present := f.executors[msgQueryID]; present {
		return executor.AddStreamedResult(msg)
	}
	return fmt.Errorf("Error in QueryResultForwarder: No query with ID %d found in executor map",
		msgQueryID.String())
}

// OptionallyCancelClientStream closes the client-side stream for the given queryID, if it still exists.
func (f *QueryResultForwarderImpl) OptionallyCancelClientStream(queryID uuid.UUID) error {
	// TODO(nserrino): Fill this in after the batch ReceiveAgentQueryResult API is deprecated
	// and querybroker/agent move fully to the streaming API.
	return nil
}
