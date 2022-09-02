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

package ptproxy

import (
	"context"
	"errors"
	"fmt"
	"io"
	"reflect"
	"sync"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/shared/cvmsgspb"
)

// PassthroughRequestChannel is the NATS channel over which stream API requests are sent.
const PassthroughRequestChannel = "c2v.VizierPassthroughRequest"

// RequestState is the state information for a stream API request.
type RequestState struct {
	requestID string             // ID of the request
	startTime time.Time          // Time the request started
	ctx       context.Context    // Context of the request
	cancel    context.CancelFunc // Cancel function for the context
}

// PassThroughProxy listens to NATS for any stream API requests and makes the necessary grpc
// requests to downstream services.
type PassThroughProxy struct {
	vzClient vizierpb.VizierServiceClient
	nc       *nats.Conn
	requests map[string]*RequestState
	mu       sync.Mutex // Mutex for requests map.
	subCh    chan *nats.Msg
	sub      *nats.Subscription
	quitCh   chan bool
}

// Stream is a wrapper around a GRPC stream.
type Stream interface {
	StartStream(ctx context.Context, requestID string, req *cvmsgspb.C2VAPIStreamRequest) error
	Recv() (*cvmsgspb.V2CAPIStreamResponse, error)
}

// NewPassThroughProxy creates a new stream API listener.
func NewPassThroughProxy(nc *nats.Conn, vzClient vizierpb.VizierServiceClient) (*PassThroughProxy, error) {
	requests := make(map[string]*RequestState)
	quitCh := make(chan bool)
	// Buffer channel so we don't drop passthrough requests.
	subCh := make(chan *nats.Msg, 4096)
	sub, err := nc.ChanSubscribe(PassthroughRequestChannel, subCh)
	if err != nil {
		return nil, err
	}
	return &PassThroughProxy{nc: nc, requests: requests, quitCh: quitCh, vzClient: vzClient, subCh: subCh, sub: sub}, nil
}

// Run starts the stream listener.
func (s *PassThroughProxy) Run() error {
	defer s.cleanup()
	for {
		select {
		case <-s.quitCh:
			log.Trace("Received quit signal for PassThroughProxy")
			return nil
		case msg := <-s.subCh:
			log.Trace("Received message in PassThroughProxy")
			go s.HandleMessage(msg)
		}
	}
}

// HandleMessage handles a stream API request or cancel request.
func (s *PassThroughProxy) HandleMessage(msg *nats.Msg) {
	err := s.handleMessage(msg)
	if err != nil {
		log.WithError(err).Error("Pass through request failed.")
	}
}

// HandleMessage handles a stream API request or cancel request.
func (s *PassThroughProxy) handleMessage(msg *nats.Msg) error {
	// Arriving messages are wrapped in a C2V message.
	c2vMsg := &cvmsgspb.C2VMessage{}
	err := proto.Unmarshal(msg.Data, c2vMsg)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal stream API request from bytes")
		return err
	}

	// Determine if it is a request or cancellation.
	req := &cvmsgspb.C2VAPIStreamRequest{}
	err = types.UnmarshalAny(c2vMsg.Msg, req)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal req message")
		return err
	}

	if req.GetCancelReq() == nil {
		err = s.handleRequest(req)
	} else {
		err = s.handleCancel(req)
	}

	return err
}

func (s *PassThroughProxy) handleRequest(req *cvmsgspb.C2VAPIStreamRequest) error {
	log.Trace("Handling C2VAPIStreamRequest")
	s.mu.Lock()
	defer s.mu.Unlock()

	if _, ok := s.requests[req.RequestID]; ok {
		log.WithField("RequestID", req.RequestID).Info("Request with ID already exists")
	} else {
		// Start go-routine to handle request.
		ctx, cancel := context.WithCancel(context.Background())
		ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
			fmt.Sprintf("bearer %s", req.Token))

		reqState := RequestState{
			requestID: req.RequestID,
			startTime: time.Now(),
			ctx:       ctx,
			cancel:    cancel,
		}

		s.requests[req.RequestID] = &reqState
		go s.runRequest(&reqState, req)
	}

	return nil
}

func (s *PassThroughProxy) handleCancel(req *cvmsgspb.C2VAPIStreamRequest) error {
	log.Trace("Handling C2VAPIStreamCancel")
	s.mu.Lock()
	defer s.mu.Unlock()
	if r, ok := s.requests[req.RequestID]; ok {
		r.cancel()
	} else {
		log.WithField("RequestID", req.RequestID).Info("Could not find request to cancel")
	}

	return nil
}

func (s *PassThroughProxy) runRequest(reqState *RequestState, msg *cvmsgspb.C2VAPIStreamRequest) {
	defer s.cleanupRequest(reqState)

	log.WithField("type", reflect.TypeOf(msg.Msg)).Info("Got passthrough request")
	var stream Stream
	switch msg.Msg.(type) {
	case *cvmsgspb.C2VAPIStreamRequest_ExecReq:
		stream = NewExecuteScriptStream(s.vzClient)
	case *cvmsgspb.C2VAPIStreamRequest_HcReq:
		stream = NewHealthCheckStream(s.vzClient)
	case *cvmsgspb.C2VAPIStreamRequest_GenerateOTelScriptReq:
		resp, err := s.vzClient.GenerateOTelScript(reqState.ctx, msg.GetGenerateOTelScriptReq())
		if err != nil {
			v2cResp := formatStatusMessage(reqState.requestID, status.Code(err), err.Error())
			s.sendMessage(reqState.requestID, v2cResp)
			return
		}
		// Wrap message in V2CAPIStreamResponse.
		s.sendMessage(reqState.requestID, &cvmsgspb.V2CAPIStreamResponse{
			RequestID: reqState.requestID,
			Msg: &cvmsgspb.V2CAPIStreamResponse_GenerateOTelScriptResp{
				GenerateOTelScriptResp: resp,
			},
		})
		s.sendMessage(reqState.requestID, formatStatusMessage(reqState.requestID, codes.OK, ""))
		return
	default:
		s.sendMessage(reqState.requestID, formatStatusMessage(reqState.requestID, codes.InvalidArgument, fmt.Sprintf("Unknown request type %s", reflect.TypeOf(msg.Msg))))
		log.Error("Unhandled message type")
		return
	}

	err := stream.StartStream(reqState.ctx, reqState.requestID, msg)
	if err != nil {
		log.WithError(err).Error("Error starting stream")
		return
	}

	for {
		msg, err := stream.Recv()
		if err != nil && err == io.EOF {
			log.Trace("Stream has closed (Read)")
			v2cResp := formatStatusMessage(reqState.requestID, codes.OK, "")
			s.sendMessage(reqState.requestID, v2cResp)
			return
		}
		if err != nil && (errors.Is(err, context.Canceled) || status.Code(err) == codes.Canceled) {
			log.Trace("Stream has been cancelled")
			v2cResp := formatStatusMessage(reqState.requestID, codes.Canceled, "")
			s.sendMessage(reqState.requestID, v2cResp)
			return
		}
		if err != nil {
			v2cResp := formatStatusMessage(reqState.requestID, status.Code(err), err.Error())
			s.sendMessage(reqState.requestID, v2cResp)
			return
		}
		log.Trace("Sending response message from stream")
		s.sendMessage(reqState.requestID, msg)
	}
}

func formatStatusMessage(reqID string, code codes.Code, message string) *cvmsgspb.V2CAPIStreamResponse {
	return &cvmsgspb.V2CAPIStreamResponse{
		RequestID: reqID,
		Msg: &cvmsgspb.V2CAPIStreamResponse_Status{
			Status: &vizierpb.Status{
				Code:    int32(code),
				Message: message,
			},
		},
	}
}

func (s *PassThroughProxy) sendMessage(reqID string, msg *cvmsgspb.V2CAPIStreamResponse) {
	topic := fmt.Sprintf("v2c.reply-%s", reqID)
	// Wrap message in V2C message.
	reqAnyMsg, err := types.MarshalAny(msg)
	if err != nil {
		log.WithError(err).Info("Failed to marshal any")
		return
	}
	v2cMsg := cvmsgspb.V2CMessage{
		Msg: reqAnyMsg,
	}
	b, err := v2cMsg.Marshal()
	if err != nil {
		log.WithError(err).Info("Failed to marshal to bytes")
		return
	}

	err = s.nc.Publish(topic, b)
	// If the err is a max payload, let's try to propagate it up, otherwise nats errors
	// mean something is broken with nats and retrying can be catastrophic.limit
	if err != nil && err == nats.ErrMaxPayload {
		errResp := formatStatusMessage(reqID, codes.Internal, "Large data batch rejected "+
			"by passthrough proxy limits. The Pixie team is currently working on this. In the meantime, please add a head() to your query to avoid the problem.")
		s.sendMessage(reqID, errResp)
	} else if err != nil {
		log.WithError(err).Error("Failed to publish message")
	}
}

func (s *PassThroughProxy) cleanupRequest(reqState *RequestState) {
	s.mu.Lock()
	defer s.mu.Unlock()

	delete(s.requests, reqState.requestID)
}

func (s *PassThroughProxy) cleanup() {
	err := s.sub.Unsubscribe()
	if err != nil {
		log.WithError(err).Error("Failed to unsubscribe")
	}
	close(s.subCh)

	s.mu.Lock()
	defer s.mu.Unlock()
	for _, v := range s.requests {
		v.cancel()
	}
}

// Close stops the API listener and cleans up any state.
func (s *PassThroughProxy) Close() {
	s.quitCh <- true
	close(s.quitCh)
}

// ExecuteScriptStream is a wrapper around the executeScript stream.
type ExecuteScriptStream struct {
	vzClient vizierpb.VizierServiceClient
	stream   vizierpb.VizierService_ExecuteScriptClient
	reqID    string
}

// NewExecuteScriptStream creates a new executeScriptStream.
func NewExecuteScriptStream(vzClient vizierpb.VizierServiceClient) *ExecuteScriptStream {
	return &ExecuteScriptStream{vzClient: vzClient}
}

// StartStream starts the ExecuteScript stream with the given request.
func (e *ExecuteScriptStream) StartStream(ctx context.Context, reqID string, req *cvmsgspb.C2VAPIStreamRequest) error {
	e.reqID = reqID
	msg := req.GetExecReq()

	stream, err := e.vzClient.ExecuteScript(ctx, msg)
	if err != nil {
		return err
	}
	e.stream = stream
	return nil
}

// Recv gets the next message on the stream.
func (e *ExecuteScriptStream) Recv() (*cvmsgspb.V2CAPIStreamResponse, error) {
	msg, err := e.stream.Recv()
	if err != nil {
		return nil, err
	}

	// Wrap message in V2CAPIStreamResponse.
	resp := &cvmsgspb.V2CAPIStreamResponse{
		RequestID: e.reqID,
		Msg: &cvmsgspb.V2CAPIStreamResponse_ExecResp{
			ExecResp: msg,
		},
	}

	return resp, nil
}

// HealthCheckStream is a wrapper around the health check stream.
type HealthCheckStream struct {
	vzClient vizierpb.VizierServiceClient
	stream   vizierpb.VizierService_HealthCheckClient
	reqID    string
}

// NewHealthCheckStream creates a new healthCheckStream.
func NewHealthCheckStream(vzClient vizierpb.VizierServiceClient) *HealthCheckStream {
	return &HealthCheckStream{vzClient: vzClient}
}

// StartStream starts the HealthCheck stream with the given request.
func (e *HealthCheckStream) StartStream(ctx context.Context, reqID string, req *cvmsgspb.C2VAPIStreamRequest) error {
	e.reqID = reqID
	msg := req.GetHcReq()

	stream, err := e.vzClient.HealthCheck(ctx, msg)
	if err != nil {
		return err
	}
	e.stream = stream
	return nil
}

// Recv gets the next message on the stream.
func (e *HealthCheckStream) Recv() (*cvmsgspb.V2CAPIStreamResponse, error) {
	msg, err := e.stream.Recv()
	if err != nil {
		return nil, err
	}

	// Wrap message in V2CAPIStreamResponse.
	resp := &cvmsgspb.V2CAPIStreamResponse{
		RequestID: e.reqID,
		Msg: &cvmsgspb.V2CAPIStreamResponse_HcResp{
			HcResp: msg,
		},
	}

	return resp, nil
}
