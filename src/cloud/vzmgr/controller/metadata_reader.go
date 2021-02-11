package controller

import (
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	"github.com/nats-io/stan.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/cloud/shared/messages"
	messagespb "pixielabs.ai/pixielabs/src/cloud/shared/messagespb"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils"
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
	k8sUID          string    // The Vizier's K8s UID.

	liveSub stan.Subscription // The subcription for the live metadata updates.
	liveCh  chan *stan.Msg

	quitCh chan struct{} // Channel to sign a stop for a particular vizier
	once   sync.Once
}

func (vz *VizierState) stop() {
	vz.once.Do(func() {
		close(vz.liveCh)
		close(vz.quitCh)
		if vz.liveSub != nil {
			vz.liveSub.Unsubscribe()
			vz.liveSub = nil
		}
	})
}

// MetadataReader reads updates from the NATS durable queue and sends updates to the indexer.
type MetadataReader struct {
	db *sqlx.DB

	sc stan.Conn
	nc *nats.Conn

	viziers map[uuid.UUID]*VizierState // Map of Vizier ID to its state.
	mu      sync.Mutex                 // Mutex for viziers map.

	quitCh chan struct{} // Channel to signal a stop for all viziers
	once   sync.Once
}

// NewMetadataReader creates a new MetadataReader.
func NewMetadataReader(db *sqlx.DB, sc stan.Conn, nc *nats.Conn) (*MetadataReader, error) {
	viziers := make(map[uuid.UUID]*VizierState)

	m := &MetadataReader{db: db, sc: sc, nc: nc, viziers: viziers, quitCh: make(chan struct{})}
	err := m.loadState()
	if err != nil {
		m.Stop()
		return nil, err
	}

	return m, nil
}

// listenForViziers listens for any newly connected Viziers and subscribes to their update channel.
func (m *MetadataReader) listenForViziers() {
	ch := make(chan *nats.Msg)
	sub, err := m.nc.ChanSubscribe(messages.VizierConnectedChannel, ch)
	if err != nil {
		log.WithError(err).Error("Failed to listen for connected viziers")
		return
	}

	defer sub.Unsubscribe()
	defer close(ch)
	for {
		select {
		case <-m.quitCh:
			log.Info("Quit signaled")
			return
		case msg := <-ch:
			vcMsg := &messagespb.VizierConnected{}
			err := proto.Unmarshal(msg.Data, vcMsg)
			if err != nil {
				log.WithError(err).Error("Could not unmarshal VizierConnected msg")
			}
			vzID := utils.UUIDFromProtoOrNil(vcMsg.VizierID)
			log.WithField("VizierID", vzID.String()).Info("Listening to metadata updates for Vizier")

			err = m.startVizierUpdates(vzID, vcMsg.ResourceVersion, vcMsg.K8sUID)
			if err != nil {
				log.WithError(err).WithField("VizierID", vzID.String()).Error("Could not start listening to updates from Vizier")
			}
		}
	}
}

func (m *MetadataReader) loadState() error {
	// Start listening for any newly connected Viziers.
	go m.listenForViziers()

	// Start listening to updates for any Viziers that are already connected to Cloud.
	err := m.listenToConnectedViziers()
	if err != nil {
		log.WithError(err).Info("Failed to load state")
		return err
	}

	return nil
}

func (m *MetadataReader) listenToConnectedViziers() error {
	query := `SELECT vizier_cluster.id, vizier_cluster.cluster_uid, vizier_index_state.resource_version
	    FROM vizier_cluster, vizier_cluster_info, vizier_index_state
	    WHERE vizier_cluster_info.vizier_cluster_id=vizier_cluster.id
	    	  AND vizier_cluster_info.vizier_cluster_id=vizier_index_state.cluster_id
	    	  AND vizier_index_state.cluster_id=vizier_cluster.id
	          AND vizier_cluster_info.status != 'DISCONNECTED'
	          AND vizier_cluster_info.status != 'UNKNOWN'`
	var val struct {
		VizierID        uuid.UUID `db:"id"`
		ResourceVersion string    `db:"resource_version"`
		K8sUID          string    `db:"cluster_uid"`
	}

	rows, err := m.db.Queryx(query)
	if err != nil {
		return err
	}
	defer rows.Close()
	for rows.Next() {
		err := rows.StructScan(&val)
		if err != nil {
			return err
		}
		err = m.startVizierUpdates(val.VizierID, val.ResourceVersion, val.K8sUID)
		if err != nil {
			return err
		}
	}

	return nil
}

// startVizierUpdates starts listening to the metadata update channel for a given vizier.
func (m *MetadataReader) startVizierUpdates(id uuid.UUID, rv string, k8sUID string) error {
	// TODO(michelle): We currently don't have to signal when a Vizier has disconnected. When we have that
	// functionality, we should clean up the Vizier map and stop its STAN subscriptions.

	m.mu.Lock()
	defer m.mu.Unlock()

	if val, ok := m.viziers[id]; ok {
		log.WithField("vizier_id", id.String()).Info("Already listening to metadata updates from Vizier")
		val.k8sUID = k8sUID // Update the K8s uid, in case it has changed with the newly connected Vizier.
		return nil
	}

	vzState := VizierState{
		id:              id,
		resourceVersion: rv,
		k8sUID:          k8sUID,
		liveCh:          make(chan *stan.Msg),
		quitCh:          make(chan struct{}),
	}

	// Subscribe to STAN topic for streaming updates.
	topic := vzshard.V2CTopic(streamingMetadataTopic, id)
	log.WithField("topic", topic).Info("Subscribing to STAN")
	liveSub, err := m.sc.Subscribe(topic, func(msg *stan.Msg) {
		vzState.liveCh <- msg
	}, stan.SetManualAckMode())
	if err != nil {
		vzState.stop()
		return err
	}

	vzState.liveSub = liveSub

	m.viziers[id] = &vzState

	go m.processVizierUpdates(&vzState)

	return nil
}

// stopVizierUpdates stops listening to the metadata update channel for a given vizier.
func (m *MetadataReader) stopVizierUpdates(id uuid.UUID) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	if vz, ok := m.viziers[id]; ok {
		vz.stop()
		delete(m.viziers, id)
		return nil
	}

	return errors.New("Vizier doesn't exist")
}

// processVizierUpdates reads from the metadata updates and sends them to the indexer in order.
func (m *MetadataReader) processVizierUpdates(vzState *VizierState) error {
	// Clean up if any error has occurred.
	defer m.stopVizierUpdates(vzState.id)

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

// Stop shuts down the metadata reader and all relevant goroutines.
func (m *MetadataReader) Stop() {
	m.once.Do(func() {
		close(m.quitCh)
	})
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

func compareResourceVersions(rv1 string, rv2 string) int {
	// The rv may end in "_#", for containers which share the same rv as pods.
	// This needs to be removed from the string that is about to be padded, and reappended after padding.
	formatRV := func(rv string) string {
		splitRV := strings.Split(rv, "_")
		paddedRV := fmt.Sprintf("%020s", splitRV[0])
		if len(splitRV) > 1 {
			// Reappend the suffix, if any.
			paddedRV = fmt.Sprintf("%s_%s", paddedRV, splitRV[1])
		}
		return paddedRV
	}

	fmtRV1 := formatRV(rv1)
	fmtRV2 := formatRV(rv2)

	if fmtRV1 == fmtRV2 {
		return 0
	}
	if fmtRV1 < fmtRV2 {
		return -1
	}
	return 1
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
	for compareResourceVersions(update.PrevResourceVersion, vzState.resourceVersion) > 0 {
		err = m.getMissingUpdates(vzState.resourceVersion, update.ResourceVersion, update.PrevResourceVersion, vzState)
		if err != nil {
			return err
		}
	}

	if compareResourceVersions(update.PrevResourceVersion, vzState.resourceVersion) == 0 {
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

func (m *MetadataReader) getMissingUpdates(from string, to string, expectedRV string, vzState *VizierState) error {
	log.WithField("vizier", vzState.id.String()).WithField("from", from).WithField("to", to).WithField("expected", expectedRV).Info("Making request for missing metadata updates")
	topicID := uuid.NewV4()
	topic := fmt.Sprintf("%s_%s", metadataResponseTopic, topicID.String())
	mdReq := &cvmsgspb.MetadataRequest{
		From:  from,
		To:    to,
		Topic: topic,
	}
	reqBytes, err := wrapMetadataRequest(vzState.id, mdReq)
	if err != nil {
		return err
	}

	// Subscribe to topic that the response will be sent on.
	subCh := make(chan *nats.Msg, 1024)
	sub, err := m.nc.ChanSubscribe(vzshard.V2CTopic(topic, vzState.id), subCh)
	defer sub.Unsubscribe()

	pubTopic := vzshard.C2VTopic(metadataRequestTopic, vzState.id)
	err = m.nc.Publish(pubTopic, reqBytes)
	if err != nil {
		return err
	}

	for {
		select {
		case <-m.quitCh:
			return errors.New("Quit signaled")
		case <-vzState.quitCh:
			return errors.New("Vizier removed")
		case msg := <-subCh:
			reqUpdates, err := readMetadataResponse(msg.Data)
			if err != nil {
				return err
			}

			updates := reqUpdates.Updates
			if len(updates) == 0 {
				return nil
			}

			if compareResourceVersions(updates[0].PrevResourceVersion, vzState.resourceVersion) > 0 {
				// Received out of order update. Need to rerequest metadata.
				log.WithField("prevRV", updates[0].PrevResourceVersion).WithField("RV", vzState.resourceVersion).Info("Received out of order update")
				return nil
			}

			err = m.applyMetadataUpdates(vzState, updates)
			if err != nil {
				return err
			}

			if compareResourceVersions(vzState.resourceVersion, expectedRV) >= 0 {
				return nil
			}
		case <-time.After(20 * time.Minute):
			// Our previous request shouldn't have gotten lost on NATS, so if there is a subscriber for the metadata
			// requests we shouldn't actually need to resend the request.
			return nil
		}
	}
}

func (m *MetadataReader) applyMetadataUpdates(vzState *VizierState, updates []*metadatapb.ResourceUpdate) error {
	for i, u := range updates {
		if compareResourceVersions(u.ResourceVersion, vzState.resourceVersion) <= 0 {
			continue // Don't send the update.
		}

		// Publish update to the indexer.
		b, err := updates[i].Marshal()
		if err != nil {
			return err
		}

		log.WithField("topic", fmt.Sprintf("%s.%s", indexerMetadataTopic, vzState.k8sUID)).WithField("rv", u.ResourceVersion).Info("Publishing metadata update to indexer")

		err = m.sc.Publish(fmt.Sprintf("%s.%s", indexerMetadataTopic, vzState.k8sUID), b)
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
