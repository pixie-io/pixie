// Package bridge connects data between the vizier NATS domain and cloud nats domain by using a GRPC channel. Each Vizier
// gets a dedicated subset of nats domain in the from v2c.<shard_id>.<cluster_id>.* and c2v.<shard_id>.<cluster_id>.*.
// v2c = vizier to cloud messages, c2v = cloud to vizier messages. The shard ID is determined by hashing the cluster_id
// and it is fixed to be values between 000 and 100.
//
// This package has the cloud counterpart to Vizier's cloud_connector/bridge component.
//
// TODO(zasgar/michelle): shards should be 00-99, will change shortly.
package bridge

import (
	"fmt"
	"io"
	"strings"
	"sync"

	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
)

// DurableNATSChannels is a list of all the durable nats channels that need to be read and transmitted over the GRPC channel.
var DurableNATSChannels = []string{"DurableMetadataRequest"}

// NATSBridgeController is responsible for routing messages from Vizier to NATS. It assumes that all authentication/handshakes
// are completed before being created.
type NATSBridgeController struct {
	streamID  uuid.UUID
	clusterID uuid.UUID
	l         *log.Entry
	srv       vzconnpb.VZConnService_NATSBridgeServer

	nc        *nats.Conn
	sc        stan.Conn
	grpcOutCh chan *vzconnpb.C2VBridgeMessage
	grpcInCh  chan *vzconnpb.V2CBridgeMessage

	quitCh chan bool      // Channel is used to signal that things should shutdown.
	wg     sync.WaitGroup // Tracks all the active goroutines.

	errCh        chan error
	subCh        chan *nats.Msg
	durableSubCh chan *stan.Msg
}

// NewNATSBridgeController creates a NATSBridgeController.
func NewNATSBridgeController(clusterID uuid.UUID, srv vzconnpb.VZConnService_NATSBridgeServer, nc *nats.Conn, sc stan.Conn) *NATSBridgeController {
	streamID := uuid.NewV4()
	return &NATSBridgeController{
		streamID:  streamID,
		clusterID: clusterID,
		l:         log.WithField("StreamID", streamID),
		srv:       srv,
		nc:        nc,
		sc:        sc,

		grpcOutCh: make(chan *vzconnpb.C2VBridgeMessage, 1000),
		grpcInCh:  make(chan *vzconnpb.V2CBridgeMessage, 1000),

		quitCh:       make(chan bool),
		errCh:        make(chan error),
		subCh:        make(chan *nats.Msg, 1000),
		durableSubCh: make(chan *stan.Msg),
	}
}

// Run is the main loop of the NATS bridge controller. This function block until Stop is called.
func (s *NATSBridgeController) Run() error {
	s.l.Info("Starting new cloud connect stream")
	defer close(s.quitCh)
	defer s.wg.Wait()
	// We need to connect to the appropriate queues based on the clusterID.
	log.WithField("ClusterID:", s.clusterID).Info("Subscribing to cluster IDs")
	topics := vzshard.C2VTopic("*", s.clusterID)
	natsSub, err := s.nc.ChanQueueSubscribe(topics, "vzconn", s.subCh)
	if err != nil {
		log.WithError(err).Error("error with ChanQueueSubscribe")
		return err
	}
	// Set large limits on message size and count.
	natsSub.SetPendingLimits(1e7, 1e7)
	defer natsSub.Unsubscribe()

	for _, topic := range DurableNATSChannels {
		sub, err := s.sc.QueueSubscribe(topic, "vzcon", func(msg *stan.Msg) {
			s.durableSubCh <- msg
		}, stan.SetManualAckMode())
		if err != nil {
			return err
		}
		defer sub.Unsubscribe()
	}

	s.wg.Add(1)
	go s.startStreamGRPCWriter()
	s.wg.Add(1)
	go s.startStreamGRPCReader()

	s.wg.Add(1)
	return s._run()
}

func (s *NATSBridgeController) _run() error {
	defer s.wg.Done()
	sendError := func(e error) {
		go func() {
			s.errCh <- e
		}()
	}

	for {
		var err error
		select {
		case err := <-s.errCh:
			if status.Code(err) == codes.Canceled {
				s.l.Info("Closing stream, context cancellation")
				return nil
			}
			s.l.WithError(err).Error("Got error, terminating stream.")
			return err
		case <-s.quitCh:
			return nil
		case msg := <-s.durableSubCh:
			s.l.WithField("msg", msg).Trace("Got durable NATS message")
			err = s.sendStanMessageToGRPC(msg)
		case msg := <-s.subCh:
			s.l.WithField("msg", msg).Trace("Got regular NATS message")
			err = s.sendNATSMessageToGRPC(msg)
		case msg := <-s.grpcInCh:
			err = s.sendMessageToMessageBus(msg)
			if err != nil {
				sendError(err)
			}
		}
		if err != nil {
			sendError(err)
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

func (s *NATSBridgeController) sendStanMessageToGRPC(msg *stan.Msg) error {
	c2vMsg := cvmsgspb.C2VMessage{}
	err := c2vMsg.Unmarshal(msg.Data)
	if err != nil {
		return err
	}
	outMsg := &vzconnpb.C2VBridgeMessage{
		Topic: s.getRemoteSubject(msg.Subject),
		Msg:   c2vMsg.Msg,
	}
	err = s.srv.Send(outMsg)
	if err != nil {
		return err
	}
	return msg.Ack()
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
	s.l.WithField("topic", topic).
		WithField("msg", outMsg.String()).
		Trace("Sending message to grpc channel")
	s.grpcOutCh <- outMsg
	return nil
}

func (s *NATSBridgeController) sendMessageToMessageBus(msg *vzconnpb.V2CBridgeMessage) error {

	natsMsg := &cvmsgspb.V2CMessage{
		VizierID:  s.clusterID.String(),
		SessionId: msg.SessionId,
		Msg:       msg.Msg,
	}
	b, err := natsMsg.Marshal()
	if err != nil {
		return err
	}
	topic := vzshard.V2CTopic(msg.Topic, s.clusterID)
	s.l.
		WithField("Message", natsMsg.String()).
		WithField("topic", topic).
		Trace("sending message to nats")

	if strings.Contains(topic, "Durable") {
		return s.sc.Publish(topic, b)
	}

	return s.nc.Publish(topic, b)

}

func (s *NATSBridgeController) startStreamGRPCReader() {
	defer s.wg.Done()
	s.l.Trace("Starting GRPC reader stream")

	for {
		select {
		case <-s.srv.Context().Done():
			return
		case <-s.quitCh:
			s.l.Info("Closing GRPC reader because of <-quit")
			return
		default:
			msg, err := s.srv.Recv()
			if err != nil && err == io.EOF {
				// stream closed.
				return
			}
			if err != nil {
				s.errCh <- err
			}
			s.grpcInCh <- msg
		}
	}
}

func (s *NATSBridgeController) startStreamGRPCWriter() {
	defer s.wg.Done()
	s.l.Trace("Starting GRPC writer stream")
	for {
		select {
		case <-s.srv.Context().Done():
			return
		case <-s.quitCh:
			s.l.Info("Closing GRPC writer because of <-quit")

			// Quit called.
			return
		case m := <-s.grpcOutCh:
			s.l.WithField("message", m.String()).Trace("Sending message over GRPC connection")
			// Write message to GRPC if it exists.
			err := s.srv.Send(m)
			if err != nil {
				s.l.WithError(err).Error("Failed to send message")
				s.errCh <- err
			}
		}
	}
}
