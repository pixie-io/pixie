package controllers

import (
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/carnotpb"
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
type QueryResultForwarderImpl struct{}

// NewQueryResultForwarder creates a new QueryResultForwarder.
func NewQueryResultForwarder() QueryResultForwarder {
	return &QueryResultForwarderImpl{}
}

// ForwardQueryResult forwards the agent result to the client result stream.
func (f *QueryResultForwarderImpl) ForwardQueryResult(msg *carnotpb.TransferResultChunkRequest) error {
	return status.Errorf(codes.Unimplemented, "method ForwardQueryResult not implemented")
}

// OptionallyCancelClientStream closes the client-side stream for the given queryID, if it still exists.
func (f *QueryResultForwarderImpl) OptionallyCancelClientStream(queryID uuid.UUID) error {
	return status.Errorf(codes.Unimplemented, "method CloseQueryResultStream not implemented")
}
