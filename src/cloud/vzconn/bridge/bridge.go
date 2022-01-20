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

// Package bridge connects data between the vizier NATS domain and cloud nats domain by using a GRPC channel. Each Vizier
// gets a dedicated subset of nats domain in the from v2c.<shard_id>.<cluster_id>.* and c2v.<shard_id>.<cluster_id>.*.
// v2c = vizier to cloud messages, c2v = cloud to vizier messages. The shard ID is determined by the first byte
// of the clusterID and is between 0x00 and 0xff.
//
// This package has the cloud counterpart to Vizier's cloud_connector/bridge component.
package bridge

import (
	"context"
	"fmt"
	"io"
	"strings"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/cloud/vzconn/vzconnpb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/services/msgbus"
)

// NATSBridgeController is responsible for routing messages from Vizier to NATS. It assumes that all authentication/handshakes
// are completed before being created.
type NATSBridgeController struct {
	streamID  uuid.UUID
	clusterID uuid.UUID
	l         *log.Entry
	srv       vzconnpb.VZConnService_NATSBridgeServer

	nc        *nats.Conn
	st        msgbus.Streamer
	grpcOutCh chan *vzconnpb.C2VBridgeMessage
	grpcInCh  chan *vzconnpb.V2CBridgeMessage

	quitCh chan bool // Channel is used to signal that things should shutdown.
	subCh  chan *nats.Msg
}

// NewNATSBridgeController creates a NATSBridgeController.
func NewNATSBridgeController(clusterID uuid.UUID, srv vzconnpb.VZConnService_NATSBridgeServer, nc *nats.Conn, st msgbus.Streamer) *NATSBridgeController {
	streamID := uuid.Must(uuid.NewV4())
	return &NATSBridgeController{
		streamID:  streamID,
		clusterID: clusterID,
		l:         log.WithField("StreamID", streamID),
		srv:       srv,
		nc:        nc,
		st:        st,

		grpcOutCh: make(chan *vzconnpb.C2VBridgeMessage, 4096),
		grpcInCh:  make(chan *vzconnpb.V2CBridgeMessage, 4096),

		quitCh: make(chan bool),
		subCh:  make(chan *nats.Msg, 4096),
	}
}

// Run is the main loop of the NATS bridge controller. This function block until Stop is called.
func (s *NATSBridgeController) Run() error {
	s.l.Info("Starting new cloud connect stream")
	defer func() {
		s.l.Trace("Closing quit")
		close(s.quitCh)
		s.l.Trace("DONE: Closing quit")
	}()

	// We need to connect to the appropriate queues based on the clusterID.
	log.WithField("ClusterID:", s.clusterID).Info("Subscribing to cluster IDs")
	topics := vzshard.C2VTopic("*", s.clusterID)
	natsSub, err := s.nc.ChanSubscribe(topics, s.subCh)

	if err != nil {
		s.l.WithError(err).Error("error with ChanQueueSubscribe")
		return err
	}
	defer func() {
		s.l.Infof("Unsubscribing from : %s", natsSub.Subject)
		err := natsSub.Unsubscribe()
		if err != nil {
			log.WithError(err).Error("Failed to properly unsubscribe")
		}
	}()

	eg, ctx := errgroup.WithContext(context.Background())
	eg.Go(func() error {
		return s.startStreamGRPCWriter(ctx)
	})
	eg.Go(func() error {
		return s.startStreamGRPCReader(ctx)
	})
	eg.Go(func() error {
		return s._run(ctx)
	})
	err = eg.Wait()
	if status.Code(err) == codes.Canceled {
		s.l.Info("Closing stream, context cancellation")
		return nil
	}

	// Return early to avoid logging this as an error, as this case is relatively frequent when
	// viziers disconnect.
	if status.Code(err) == codes.Unavailable {
		return err
	}

	s.l.WithError(err).Error("Closing stream with error")
	return err
}

func (s *NATSBridgeController) _run(ctx context.Context) error {
	for {
		var err error
		select {
		case <-s.quitCh:
			return nil
		case msg := <-s.subCh:
			msgKind := cleanCloudToVizierMessageKind(msg.Subject)
			cloudToVizierMsgCount.
				WithLabelValues(s.clusterID.String(), msgKind).
				Inc()
			cloudToVizierMsgSizeDist.
				WithLabelValues(msgKind).
				Observe(float64(len(msg.Data)))

			err = s.sendNATSMessageToGRPC(msg)
		case msg := <-s.grpcInCh:
			msgKind := cleanVizierToCloudMessageKind(msg.Topic)
			vizierToCloudMsgCount.
				WithLabelValues(s.clusterID.String(), msgKind).
				Inc()
			vizierToCloudMsgSizeDist.
				WithLabelValues(msgKind).
				Observe(float64(len(msg.Msg.Value)))

			err = s.sendMessageToMessageBus(msg)
		case <-ctx.Done():
			return ctx.Err()
		}
		if err != nil {
			return err
		}
	}
}

func (s *NATSBridgeController) getRemoteSubject(topic string) string {
	prefix := fmt.Sprintf("c2v.%s.", s.clusterID.String())
	if strings.HasPrefix(topic, prefix) {
		return strings.TrimPrefix(topic, prefix)
	}
	return ""
}

func (s *NATSBridgeController) sendNATSMessageToGRPC(msg *nats.Msg) error {
	c2vMsg := cvmsgspb.C2VMessage{}
	err := c2vMsg.Unmarshal(msg.Data)
	if err != nil {
		return err
	}

	topic := s.getRemoteSubject(msg.Subject)

	outMsg := &vzconnpb.C2VBridgeMessage{
		Topic: topic,
		Msg:   c2vMsg.Msg,
	}

	s.grpcOutCh <- outMsg
	return nil
}

func (s *NATSBridgeController) sendMessageToMessageBus(msg *vzconnpb.V2CBridgeMessage) error {
	cid := s.clusterID.String()
	natsMsg := &cvmsgspb.V2CMessage{
		VizierID:  cid,
		SessionId: msg.SessionId,
		Msg:       msg.Msg,
	}
	b, err := natsMsg.Marshal()
	if err != nil {
		return err
	}
	topic := vzshard.V2CTopic(msg.Topic, s.clusterID)

	if strings.Contains(topic, "Durable") {
		stanPublishCount.WithLabelValues(cid).Inc()
		return s.st.Publish(topic, b)
	}

	natsPublishCount.WithLabelValues(cid).Inc()
	return s.nc.Publish(topic, b)
}

func (s *NATSBridgeController) startStreamGRPCReader(ctx context.Context) error {
	s.l.Trace("Starting GRPC reader stream")

	for {
		select {
		case <-s.srv.Context().Done():
			return nil
		case <-s.quitCh:
			s.l.Info("Closing GRPC reader because of <-quit")
			return nil
		case <-ctx.Done():
			return ctx.Err()
		default:
			msg, err := s.srv.Recv()
			if err != nil && err == io.EOF {
				// stream closed.
				return nil
			}
			if err != nil {
				return err
			}
			s.grpcInCh <- msg
		}
	}
}

func (s *NATSBridgeController) startStreamGRPCWriter(ctx context.Context) error {
	s.l.Trace("Starting GRPC writer stream")
	for {
		select {
		case <-s.srv.Context().Done():
			return nil
		case <-s.quitCh:
			s.l.Info("Closing GRPC writer because of <-quit")

			// Quit called.
			return nil
		case <-ctx.Done():
			return ctx.Err()
		case m := <-s.grpcOutCh:
			// Write message to GRPC if it exists.
			err := s.srv.Send(m)
			if err != nil {
				s.l.WithError(err).Error("Failed to send message")
				return err
			}
		}
	}
}
