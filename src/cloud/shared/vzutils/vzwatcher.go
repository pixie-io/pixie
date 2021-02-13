package vzutils

import (
	"context"
	"fmt"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/metadata"

	"pixielabs.ai/pixielabs/src/cloud/shared/messages"
	messagespb "pixielabs.ai/pixielabs/src/cloud/shared/messagespb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	svcutils "pixielabs.ai/pixielabs/src/shared/services/utils"
	"pixielabs.ai/pixielabs/src/utils"
)

// VizierHandlerFn is the function signature for the function that is run for active and newly connected Viziers.
type VizierHandlerFn func(id uuid.UUID, orgID uuid.UUID, uid string) error

// ErrorHandlerFn is the function signature for the function that should be called when the VizierHandlerFn returns
// an error.
type ErrorHandlerFn func(id uuid.UUID, orgID uuid.UUID, uid string, err error)

// Watcher tracks active Viziers and executes the registered task for each Vizier.
type Watcher struct {
	nc          *nats.Conn
	vzmgrClient vzmgrpb.VZMgrServiceClient

	vizierHandlerFn VizierHandlerFn
	errorHandlerFn  ErrorHandlerFn

	quitCh      chan bool
	ch          chan *nats.Msg
	sub         *nats.Subscription
	toShardID   string
	fromShardID string
}

// NewWatcher creates a new vizier watcher.
func NewWatcher(nc *nats.Conn, vzmgrClient vzmgrpb.VZMgrServiceClient, fromShardID string, toShardID string) (*Watcher, error) {
	ch := make(chan *nats.Msg, 1024)
	sub, err := nc.ChanSubscribe(messages.VizierConnectedChannel, ch)
	if err != nil {
		close(ch)
		return nil, err
	}

	vw := &Watcher{
		nc:          nc,
		vzmgrClient: vzmgrClient,
		quitCh:      make(chan bool),
		ch:          ch,
		sub:         sub,
		toShardID:   toShardID,
		fromShardID: fromShardID,
	}

	go vw.runWatch()

	return vw, nil
}

// runWatch subscribes to the NATS channel for any newly connected viziers, and executes the registered task for
// each.
func (w *Watcher) runWatch() {
	defer w.sub.Unsubscribe()
	defer close(w.ch)
	for {
		select {
		case <-w.quitCh:
			log.Info("Quit signaled")
			return
		case msg := <-w.ch:
			vcMsg := &messagespb.VizierConnected{}
			err := proto.Unmarshal(msg.Data, vcMsg)
			if err != nil {
				log.WithError(err).Error("Could not unmarshal VizierConnected msg")
				continue
			}
			vzID := utils.UUIDFromProtoOrNil(vcMsg.VizierID)
			orgID := utils.UUIDFromProtoOrNil(vcMsg.OrgID)
			w.onVizier(vzID, orgID, vcMsg.K8sUID)
		}
	}
}

func (w *Watcher) onVizier(id uuid.UUID, orgID uuid.UUID, uid string) {
	if w.vizierHandlerFn == nil {
		return
	}

	err := w.vizierHandlerFn(id, orgID, uid)
	if err != nil && w.errorHandlerFn != nil {
		w.errorHandlerFn(id, orgID, uid, err)
	}
}

// RegisterVizierHandler registers the function that should be called on all currently active Viziers, and any newly
// connected Viziers.
func (w *Watcher) RegisterVizierHandler(fn VizierHandlerFn) error {
	w.vizierHandlerFn = fn

	// Call VizierHandlerFn on all current-active viziers.
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return err
	}
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))
	vzmgrResp, err := w.vzmgrClient.GetViziersByShard(ctx, &vzmgrpb.GetViziersByShardRequest{
		FromShardID: w.fromShardID,
		ToShardID:   w.toShardID,
	})
	if err != nil {
		return err
	}

	for _, vz := range vzmgrResp.Viziers {
		vzID := utils.UUIDFromProtoOrNil(vz.VizierID)
		orgID := utils.UUIDFromProtoOrNil(vz.OrgID)
		w.onVizier(vzID, orgID, vz.K8sUID)
	}

	return nil
}

// RegisterErrorHandler registers the function that should be called when the VizierHandler returns an error.
func (w *Watcher) RegisterErrorHandler(fn ErrorHandlerFn) {
	w.errorHandlerFn = fn
}

// Stop stops the watcher.
func (w *Watcher) Stop() {
	close(w.quitCh)
}

func getServiceCredentials(signingKey string) (string, error) {
	claims := svcutils.GenerateJWTForService("vzwatcher")
	return svcutils.SignJWTClaims(claims, signingKey)
}
