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

package grpc

import (
	"context"
	"fmt"
	"io"
	"net"
	"strings"

	"google.golang.org/grpc"

	loadtestpb "px.dev/pixie/src/e2e_test/protocol_loadtest/grpc/loadtestpb"
)

type loadTestServer struct {
}

// Implements the Unary loadtest endpoint.
func (lt *loadTestServer) Unary(ctx context.Context, req *loadtestpb.UnaryRequest) (*loadtestpb.UnaryReply, error) {
	return &loadtestpb.UnaryReply{
		SeqId:   req.SeqId,
		Payload: strings.Repeat("u", int(req.BytesRequested)),
	}, nil
}

// Implements the ClientStreaming loadtest endpoint.
func (lt *loadTestServer) ClientStreaming(s loadtestpb.LoadTester_ClientStreamingServer) error {
	seq := int64(0)
	bytesRequested := 0

	for {
		select {
		case <-s.Context().Done():
			return nil
		default:
			msg, err := s.Recv()
			if msg != nil {
				bytesRequested = int(msg.BytesRequested)
				seq = msg.SeqId
			}
			if err != nil && err == io.EOF {
				return s.SendAndClose(&loadtestpb.ClientStreamingReply{
					SeqId:   seq,
					Payload: strings.Repeat("c", bytesRequested),
				})
			}
			if err != nil {
				return err
			}
		}
	}
}

// Implements the ServerStreaming loadtest endpoint.
func (lt *loadTestServer) ServerStreaming(req *loadtestpb.ServerStreamingRequest, s loadtestpb.LoadTester_ServerStreamingServer) error {
	payload := strings.Repeat("s", int(req.BytesRequestedPerMessage))
	for i := int32(0); i < req.MessagesRequested; i++ {
		resp := &loadtestpb.ServerStreamingReply{
			SeqId:       req.SeqId,
			StreamSeqId: int64(i),
			Payload:     payload,
		}
		if err := s.Send(resp); err != nil {
			return err
		}
	}
	return nil
}

// Implements the BidirectionalStreaming loadtest endpoint.
func (lt *loadTestServer) BidirectionalStreaming(s loadtestpb.LoadTester_BidirectionalStreamingServer) error {
	msgsSent := int64(0)
	for {
		in, err := s.Recv()
		if err == io.EOF {
			return nil
		}
		if err != nil {
			return err
		}
		resp := &loadtestpb.BidirectionalStreamingReply{
			SeqId:             in.SeqId,
			StreamSeqIdServer: msgsSent,
			Payload:           strings.Repeat("b", int(in.BytesRequestedNextMessage)),
		}
		msgsSent++
		if err := s.Send(resp); err != nil {
			return err
		}
	}
}

// RunGRPCServers sets up and runs the SSL (not yet added) and non-SSL GRPC servers.
// TODO(nserrino): PP-3238 Add SSL and gzip support.
func RunGRPCServers(port string) {
	lis, err := net.Listen("tcp", fmt.Sprintf(":%s", port))
	if err != nil {
		panic(fmt.Sprintf("failed to listen: %v", err))
	}
	var opts []grpc.ServerOption
	grpcServer := grpc.NewServer(opts...)
	loadtestpb.RegisterLoadTesterServer(grpcServer, &loadTestServer{})
	err = grpcServer.Serve(lis)
	if err != nil {
		panic(err)
	}
}
