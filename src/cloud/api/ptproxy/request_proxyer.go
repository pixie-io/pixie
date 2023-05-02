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
	"fmt"
	"io"
	"strings"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
)

var (
	// ErrNotAvailable is he error produced when vizier is not yet available.
	ErrNotAvailable = status.Error(codes.Unavailable, "cluster is not in a healthy state")
	// ErrCredentialFetch occurs when we can't fetch credentials for a vizier.
	ErrCredentialFetch = status.Error(codes.Internal, "failed to fetch creds for cluster")
	// ErrCredentialGenerate occurs when we can't generate new credentials using the cluster key.
	ErrCredentialGenerate = status.Error(codes.Internal, "failed to generate creds for cluster")
	// ErrPermissionDenied occurs when permission is denied to the cluster.
	ErrPermissionDenied = status.Error(codes.PermissionDenied, "permission denied for access to cluster")
)

// requestProxyer manages a single proxy request.
type requestProxyer struct {
	clusterID         uuid.UUID
	requestID         uuid.UUID
	signedVizierToken string
	sub               *nats.Subscription
	nc                *nats.Conn
	natsCh            chan *nats.Msg
	ctx               context.Context
	srv               grpcStream
}

type grpcStream interface {
	Context() context.Context
	SendMsg(interface{}) error
}

// ClusterIDer is used to get cluster ID from request.
type ClusterIDer interface {
	GetClusterID() string
}

func newRequestProxyer(vzmgr vzmgrClient, nc *nats.Conn, debugMode bool, r ClusterIDer, s grpcStream) (*requestProxyer, error) {
	// Make a request to Vizier Manager validate that we have permissions to access the cluster
	// and get the key to generate the token.
	requestID, err := uuid.NewV4()
	if err != nil {
		return nil, err
	}
	ctx := s.Context()
	token, _, err := getCredsFromCtx(ctx)
	if err != nil {
		return nil, err
	}

	p := &requestProxyer{requestID: requestID, nc: nc}

	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", token))

	var clusterID uuid.UUID
	if clusterID, err = uuid.FromString(r.GetClusterID()); err != nil {
		return nil, status.Error(codes.InvalidArgument, "missing/malformed cluster_id")
	}
	p.clusterID = clusterID

	signedToken, err := p.validateRequestAndFetchCreds(ctx, debugMode, vzmgr)
	if err != nil {
		if err == ErrNotAvailable {
			return nil, err
		}
		return nil, ErrCredentialFetch
	}

	// Now that everything is setup, we are ready to make the request and wait for the replies.
	// To start the request we need to send a request on the C2V channel. We also will subscribe to the reply nats channel first
	// to avoid any races.
	replyTopic := p.getRecvTopic()
	// We create a buffered channel to make sure we don't drop messages if we fall behind processing them.
	// This is not a permanent fix because we need to be more robust to this condition.
	natsCh := make(chan *nats.Msg, 4096)
	sub, err := p.nc.ChanSubscribe(replyTopic, natsCh)
	if err != nil {
		return nil, status.Error(codes.Internal, "Failed to listen for message")
	}

	p.signedVizierToken = signedToken
	p.natsCh = natsCh
	p.ctx = ctx
	p.sub = sub
	p.srv = s

	return p, nil
}

func (p requestProxyer) validateRequestAndFetchCreds(ctx context.Context, debugMode bool, vzmgr vzmgrClient) (string, error) {
	var signingKey string
	clusterIDProto := utils.ProtoFromUUID(p.clusterID)

	eg := errgroup.Group{}
	eg.Go(func() error {
		resp, err := vzmgr.GetVizierInfo(ctx, clusterIDProto)
		if err != nil {
			if status.Code(err) == codes.Unauthenticated {
				return ErrPermissionDenied
			}
			return err
		}
		if debugMode {
			if resp.Status == cvmsgspb.VZ_ST_DISCONNECTED {
				return ErrNotAvailable
			}
		} else {
			if resp.Status != cvmsgspb.VZ_ST_HEALTHY && resp.Status != cvmsgspb.VZ_ST_DEGRADED {
				return ErrNotAvailable
			}
		}

		return nil
	})

	eg.Go(func() error {
		cInfo, err := vzmgr.GetVizierConnectionInfo(ctx, clusterIDProto)
		if err != nil {
			return err
		}
		signingKey = cInfo.Token
		return nil
	})

	return signingKey, eg.Wait()
}

func (p requestProxyer) getSendTopic() string {
	return vzshard.C2VTopic("VizierPassthroughRequest", p.clusterID)
}

func (p requestProxyer) getRecvTopic() string {
	return vzshard.V2CTopic(fmt.Sprintf("reply-%s", p.requestID.String()), p.clusterID)
}

func (p requestProxyer) prepareVizierRequest() *cvmsgspb.C2VAPIStreamRequest {
	return &cvmsgspb.C2VAPIStreamRequest{
		RequestID: p.requestID.String(),
		Token:     p.signedVizierToken,
	}
}

func (p requestProxyer) marshallAsC2VMsg(msg proto.Message) ([]byte, error) {
	anyPB, err := types.MarshalAny(msg)
	if err != nil {
		return nil, err
	}

	cvmsg := &cvmsgspb.C2VMessage{
		VizierID: p.clusterID.String(),
		Msg:      anyPB,
	}

	return cvmsg.Marshal()
}

func (p *requestProxyer) sendCancelMessageToVizier() error {
	msg := p.prepareVizierRequest()
	msg.Msg = &cvmsgspb.C2VAPIStreamRequest_CancelReq{CancelReq: &cvmsgspb.C2VAPIStreamCancel{}}

	return p.sendMessageToVizier(msg)
}

func (p *requestProxyer) Run() error {
	eg := errgroup.Group{}
	eg.Go(p.run)
	return eg.Wait()
}

func (p *requestProxyer) run() error {
	for {
		select {
		case <-p.ctx.Done():
			log.Trace("Context done, sending stream cancel")
			// Send the cancel and terminate the stream. No need to wait for reply.
			err := p.sendCancelMessageToVizier()
			if err != nil {
				log.WithError(err).Error("Failed to send query cancel message")
				return err
			}
			return p.ctx.Err()
		case msg := <-p.natsCh:
			// Incoming message from vizier.
			if msg == nil {
				log.Trace("Got empty message from nats, assuming eos")
				return nil
			}
			if err := p.processNatsMsg(msg); err != nil {
				// Stream ended.
				if err == io.EOF {
					return nil
				}

				// These errors happen frequently, for example, if a Kelvin isn't ready for a cluster yet, or there is slight clock skew on the Vizier. Do not log an error in these situations.
				if !strings.Contains(err.Error(), "InvalidArgument") && !strings.Contains(err.Error(), "Unauthenticated") {
					log.WithField("vizier", p.clusterID).WithError(err).Error("Failed to process nats message")
				}
				// Try to cancel stream.
				if cancelErr := p.sendCancelMessageToVizier(); cancelErr != nil {
					log.WithError(cancelErr).Error("Failed to cancel stream")
				}
				return err
			}
		}
	}
}

// Finish finalizes the request proxyer and cleans up resources. Must be called to prevent resource leak.
func (p *requestProxyer) Finish() {
	err := p.sub.Unsubscribe()
	if err != nil {
		log.WithError(err).Error("requestProxyer failed to unsubscribe")
	}
}

func (p requestProxyer) sendMessageToVizier(req *cvmsgspb.C2VAPIStreamRequest) error {
	b, err := p.marshallAsC2VMsg(req)
	if err != nil {
		return err
	}

	topic := p.getSendTopic()
	return p.nc.Publish(topic, b)
}

func (p *requestProxyer) processNatsMsg(msg *nats.Msg) error {
	v2c := cvmsgspb.V2CMessage{}
	err := v2c.Unmarshal(msg.Data)
	if err != nil {
		return fmt.Errorf("Failed to unmarshall v2cMessage: %w", err)
	}

	resp := cvmsgspb.V2CAPIStreamResponse{}
	err = types.UnmarshalAny(v2c.Msg, &resp)
	if err != nil {
		return fmt.Errorf("Failed to unmarshall v2cAPIStreamResponse: %w", err)
	}

	switch parsed := resp.Msg.(type) {
	case *cvmsgspb.V2CAPIStreamResponse_ExecResp:
		err = p.srv.SendMsg(parsed.ExecResp)
		if err != nil {
			return fmt.Errorf("Failed to send ExecResp message: %w", err)
		}
	case *cvmsgspb.V2CAPIStreamResponse_HcResp:
		err = p.srv.SendMsg(parsed.HcResp)
		if err != nil {
			return fmt.Errorf("Failed to send HcResp message: %w", err)
		}
	case *cvmsgspb.V2CAPIStreamResponse_GenerateOTelScriptResp:
		err = p.srv.SendMsg(parsed.GenerateOTelScriptResp)
		if err != nil {
			return fmt.Errorf("Failed to send GenerateOTelScriptResp message: %w", err)
		}
	case *cvmsgspb.V2CAPIStreamResponse_DebugLogResp:
		err = p.srv.SendMsg(parsed.DebugLogResp)
		if err != nil {
			return fmt.Errorf("Failed to send DebugLogResp message: %w", err)
		}
	case *cvmsgspb.V2CAPIStreamResponse_DebugPodsResp:
		err = p.srv.SendMsg(parsed.DebugPodsResp)
		if err != nil {
			return fmt.Errorf("Failed to send DebugPodsResp message: %w", err)
		}
	case *cvmsgspb.V2CAPIStreamResponse_Status:
		// Status message come when the stream is closed.
		if codes.Code(parsed.Status.Code) == codes.OK {
			return io.EOF
		}
		return status.Error(codes.Code(parsed.Status.Code), parsed.Status.Message)
	default:
		log.WithField("type", parsed).
			Error("Got unexpected message type")
		return status.Error(codes.Internal, "Got invalid message")
	}
	return nil
}
