package controller

import (
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
)

// The topic on which to make metadata requests.
const metadataRequestTopic = "MetadataRequest"

// The topic on which to listen to metadata responses.
const metadataResponseTopic = "MetadataResponse"

// The topic on which to listen for streaming metadata updates.
const streamingMetadataTopic = "DurableMetadataUpdates"

// The topic on which to write updates to.
const indexerMetadataTopic = "MetadataIndex"

// VizierState contains all state necessary to process metadata updates for the given vizier.
type VizierState struct {
	id              uuid.UUID // The Vizier's ID.
	resourceVersion string    // The Vizier's last applied resource version.

	liveSub stan.Subscription // The subcription for the live metadata updates.
	liveCh  chan *stan.Msg

	quitCh chan bool // Channel to sign a stop for a particular vizier
}

// MetadataReader reads updates from the NATS durable queue and sends updates to the indexer.
type MetadataReader struct {
	db *sqlx.DB

	sc stan.Conn
	nc *nats.Conn

	viziers map[uuid.UUID]*VizierState // Map of Vizier ID to its state.
	mu      sync.Mutex                 // Mutex for viziers map.

	quitCh chan bool // Channel to signal a stop for all viziers
}

// NewMetadataReader creates a new MetadataReader.
func NewMetadataReader(db *sqlx.DB, sc stan.Conn, nc *nats.Conn) (*MetadataReader, error) {
	viziers := make(map[uuid.UUID]*VizierState)

	m := &MetadataReader{db: db, sc: sc, nc: nc, viziers: viziers}
	err := m.loadState()
	if err != nil {
		return nil, err
	}

	return m, nil
}

func (m *MetadataReader) loadState() error {
	// TOOD(michelle): This should get all currently running Viziers and run StartVizierUpdates() for each of them.
	// This will come in a followup diff.
	return nil
}

// StartVizierUpdates starts listening to the metadata update channel for a given vizier.
func (m *MetadataReader) StartVizierUpdates(id uuid.UUID, rv string) error {
	// TODO(michelle): We currently don't have to signal when a Vizier has disconnected. When we have that
	// functionality, we should clean up the Vizier map and stop its STAN subscriptions.

	m.mu.Lock()
	defer m.mu.Unlock()

	if _, ok := m.viziers[id]; ok {
		log.WithField("vizier_id", id.String()).Info("Already listening to metadata updates from Vizier")
		return nil
	}

	vzState := VizierState{
		id:              id,
		resourceVersion: rv,
		liveCh:          make(chan *stan.Msg),
		quitCh:          make(chan bool),
	}

	// Subscribe to STAN topic for streaming updates.
	topic := vzshard.V2CTopic(streamingMetadataTopic, id)
	log.WithField("topic", topic).Info("Subscribing to STAN")
	liveSub, err := m.sc.Subscribe(topic, func(msg *stan.Msg) {
		vzState.liveCh <- msg
	}, stan.SetManualAckMode())
	if err != nil {
		close(vzState.liveCh)
		close(vzState.quitCh)
		return err
	}

	vzState.liveSub = liveSub

	m.viziers[id] = &vzState

	go m.ProcessVizierUpdates(&vzState)

	return nil
}

// StopVizierUpdates stops listening to the metadata update channel for a given vizier.
func (m *MetadataReader) StopVizierUpdates(id uuid.UUID) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	if vz, ok := m.viziers[id]; ok {
		vz.liveSub.Unsubscribe()
		close(vz.liveCh)
		close(vz.quitCh)

		delete(m.viziers, id)
		return nil
	}

	return errors.New("Vizier doesn't exist")
}

// ProcessVizierUpdates reads from the metadata updates and sends them to the indexer in order.
func (m *MetadataReader) ProcessVizierUpdates(vzState *VizierState) error {
	// Clean up if any error has occurred.
	defer m.StopVizierUpdates(vzState.id)

	for {
		select {
		case <-m.quitCh:
			return nil
		case <-vzState.quitCh:
			return nil
		case msg := <-vzState.liveCh:
			err := m.processVizierUpdate(msg, vzState)
			if err != nil {
				log.WithError(err).Error("Error processing Vizier metadata updates")
				return err
			}
		}
	}
}

func readMetadataUpdate(data []byte) (*cvmsgspb.MetadataUpdate, error) {
	v2cMsg := &cvmsgspb.V2CMessage{}
	err := proto.Unmarshal(data, v2cMsg)
	if err != nil {
		return nil, err
	}
	updateMsg := &cvmsgspb.MetadataUpdate{}
	err = types.UnmarshalAny(v2cMsg.Msg, updateMsg)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal metadata update message")
		return nil, err
	}
	return updateMsg, nil
}

func readMetadataResponse(data []byte) (*cvmsgspb.MetadataResponse, error) {
	v2cMsg := &cvmsgspb.V2CMessage{}
	err := proto.Unmarshal(data, v2cMsg)
	if err != nil {
		return nil, err
	}
	reqUpdates := &cvmsgspb.MetadataResponse{}
	err = types.UnmarshalAny(v2cMsg.Msg, reqUpdates)
	if err != nil {
		log.WithError(err).Error("Could not unmarshal metadata response message")
		return nil, err
	}
	return reqUpdates, nil
}

func wrapMetadataRequest(vizierID uuid.UUID, req *cvmsgspb.MetadataRequest) ([]byte, error) {
	reqAnyMsg, err := types.MarshalAny(req)
	if err != nil {
		return nil, err
	}
	c2vMsg := cvmsgspb.C2VMessage{
		VizierID: vizierID.String(),
		Msg:      reqAnyMsg,
	}
	b, err := c2vMsg.Marshal()
	if err != nil {
		return nil, err
	}
	return b, nil
}

func (m *MetadataReader) processVizierUpdate(msg *stan.Msg, vzState *VizierState) error {
	if msg == nil {
		return nil
	}

	updateMsg, err := readMetadataUpdate(msg.Data)
	if err != nil {
		return err
	}
	update := updateMsg.Update

	if update.PrevResourceVersion == "" || update.PrevResourceVersion > vzState.resourceVersion {
		// If update.PrevResourceVersion == "", we just received an update from a vizier which has just
		// started up. We will need to fetch all updates from vzState.resourceVersion to the update's resourceVersion.
		// We received an update later than we one we need next. Send out a request for the missing updates.
		updatesMsg, err := m.getMissingUpdates(vzState.resourceVersion, update.ResourceVersion, vzState)
		if err != nil {
			return err
		}

		reqUpdates, err := readMetadataResponse(updatesMsg.Data)
		if err != nil {
			return err
		}

		allUpdates := append(reqUpdates.Updates, update)
		err = m.applyMetadataUpdates(vzState, allUpdates)
		if err != nil {
			return err
		}
	} else if update.PrevResourceVersion == vzState.resourceVersion {
		// Update is in correct order, so send off the update.
		err := m.applyMetadataUpdates(vzState, []*metadatapb.ResourceUpdate{update})
		if err != nil {
			return err
		}
	}

	// It's possible that we've missed the message's 30-second ACK timeline, if it took a while to receive
	// missing metadata updates. This message will be sent back on STAN for reprocessing, but we will
	// ignore it since the ResourceVersion will be less than the current resource
	msg.Ack()
	return nil
}

func (m *MetadataReader) getMissingUpdates(from string, to string, vzState *VizierState) (*nats.Msg, error) {
	log.WithField("vizier", vzState.id.String()).WithField("from", from).WithField("to", to).Info("Making request for missing metadata updates")

	topicID := uuid.NewV4()
	topic := fmt.Sprintf("%s_%s", metadataResponseTopic, topicID.String())

	mdReq := &cvmsgspb.MetadataRequest{
		From:  from,
		To:    to,
		Topic: topic,
	}
	reqBytes, err := wrapMetadataRequest(vzState.id, mdReq)
	if err != nil {
		return nil, err
	}

	// Subscribe to topic that the response will be sent on.
	subCh := make(chan *nats.Msg)
	sub, err := m.nc.ChanSubscribe(vzshard.V2CTopic(topic, vzState.id), subCh)
	defer sub.Unsubscribe()

	pubTopic := vzshard.C2VTopic(metadataRequestTopic, vzState.id)
	err = m.nc.Publish(pubTopic, reqBytes)
	if err != nil {
		return nil, err
	}

	for {
		select {
		case <-m.quitCh:
			return nil, errors.New("Quit signaled")
		case <-vzState.quitCh:
			return nil, errors.New("Vizier removed")
		case msg := <-subCh:
			return msg, nil
		case <-time.After(20 * time.Minute):
			// Our previous request shouldn't have gotten lost on NATS, so if there is a subscriber for the metadata
			// requests we shouldn't actually need to resend the request.
			log.WithField("vizier", vzState.id.String()).Info("Retrying missing metadata request")
			err = m.nc.Publish(pubTopic, reqBytes)
			if err != nil {
				return nil, err
			}
		}
	}
}

func (m *MetadataReader) applyMetadataUpdates(vzState *VizierState, updates []*metadatapb.ResourceUpdate) error {
	for i, u := range updates {
		if u.ResourceVersion <= vzState.resourceVersion {
			continue // Don't send the update.
		}

		// Publish update to the indexer.
		b, err := updates[i].Marshal()
		if err != nil {
			return err
		}

		log.WithField("topic", indexerMetadataTopic).WithField("rv", u.ResourceVersion).Info("Publishing metadata update to indexer")

		err = m.sc.Publish(indexerMetadataTopic, b)
		if err != nil {
			return err
		}

		vzState.resourceVersion = u.ResourceVersion

		// Update index state in Postgres.
		query := `
	    UPDATE vizier_index_state
	    SET resource_version = $2
	    WHERE cluster_id = $1`
		row, err := m.db.Queryx(query, vzState.id, u.ResourceVersion)
		if err != nil {
			return err
		}
		row.Close()
	}
	return nil
}
