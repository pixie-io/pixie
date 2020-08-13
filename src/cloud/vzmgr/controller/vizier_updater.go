package controller

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/metadata"

	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	versionspb "pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	jwtutils "pixielabs.ai/pixielabs/src/shared/services/utils"
	"pixielabs.ai/pixielabs/src/utils"
)

// Updater is responsible for tracking and updating Viziers.
type Updater struct {
	latestVersion string // The latest Vizier version.

	db       *sqlx.DB
	atClient artifacttrackerpb.ArtifactTrackerClient
	nc       *nats.Conn

	quitCh chan bool
}

// NewUpdater creates a new Vizier updater.
func NewUpdater(db *sqlx.DB, atClient artifacttrackerpb.ArtifactTrackerClient, nc *nats.Conn) (*Updater, error) {
	updater := &Updater{
		db:       db,
		atClient: atClient,
		nc:       nc,
		quitCh:   make(chan bool),
	}

	latestVersion, err := updater.getLatestVizierVersion()
	if err != nil {
		return nil, err
	}

	updater.latestVersion = latestVersion

	go updater.pollVizierVersion()

	return updater, nil
}

// Poll periodically to see if the Vizier version has updated.
func (u *Updater) pollVizierVersion() {
	tick := time.Tick(30 * time.Minute)
	for {
		select {
		case <-u.quitCh:
			log.Info("Quit signal, stopping Vizier version polling")
			return
		case <-tick:
			vzVersion, err := u.getLatestVizierVersion()
			if err == nil {
				u.latestVersion = vzVersion
			}
		}
	}
}

func (u *Updater) getLatestVizierVersion() (string, error) {
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return "", errors.New("Could not get service creds")
	}
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	req := &artifacttrackerpb.GetArtifactListRequest{
		ArtifactName: "vizier",
		ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
		Limit:        1,
	}

	resp, err := u.atClient.GetArtifactList(ctx, req)
	if err != nil {
		return "", err
	}

	if len(resp.Artifact) != 1 {
		return "", errors.New("Could not find Vizier artifact")
	}

	return resp.Artifact[0].VersionStr, nil
}

// UpdateOrInstallVizier immediately updates or installs the Vizier instance. This should be used in cases where
// the user is bootstrapping Vizier for the first time, or has manually sent an update request.
func (u *Updater) UpdateOrInstallVizier(vizierID uuid.UUID, version string, redeployEtcd bool) (*cvmsgspb.V2CMessage, error) {
	return u.updateOrInstallVizier(vizierID, version, redeployEtcd)
}

// Helper method for updating/installing a Vizier instance.
func (u *Updater) updateOrInstallVizier(vizierID uuid.UUID, version string, redeployEtcd bool) (*cvmsgspb.V2CMessage, error) {
	// Set up ctx.
	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, errors.New("Could not get service creds")
	}
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	// Validate version.
	if version == "" {
		version = u.latestVersion
	} else {
		atReq := &artifacttrackerpb.GetDownloadLinkRequest{
			ArtifactName: "vizier",
			VersionStr:   version,
			ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
		}

		_, err = u.atClient.GetDownloadLink(ctx, atReq)
		if err != nil {
			log.WithError(err).Error("Incorrect version")
			return nil, errors.New("Invalid version")
		}
	}

	// Generate token.
	clusterClaims := jwtutils.GenerateJWTForCluster(vizierID.String())
	tokenString, err := jwtutils.SignJWTClaims(clusterClaims, viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, errors.New("Could not generate Vizier token")
	}

	// Update state in DB.
	query := `UPDATE vizier_cluster_info SET status = 'UPDATING' WHERE vizier_cluster_id = $1`
	_, err = u.db.Exec(query, vizierID)
	if err != nil {
		return nil, errors.New("Could not update Vizier status")
	}

	// Send a message to the correct vizier that it should be updated.
	req := &cvmsgspb.UpdateOrInstallVizierRequest{
		VizierID:     utils.ProtoFromUUID(&vizierID),
		Version:      version,
		Token:        tokenString,
		RedeployEtcd: redeployEtcd,
	}
	reqAnyMsg, err := types.MarshalAny(req)
	if err != nil {
		return nil, err
	}

	// Subscribe to topic that the response will be sent on.
	subCh := make(chan *nats.Msg, 1024)
	sub, err := u.nc.ChanSubscribe(vzshard.V2CTopic("VizierUpdateResponse", vizierID), subCh)
	defer sub.Unsubscribe()

	log.WithField("Vizier ID", vizierID.String()).WithField("version", req.Version).Info("Sending update request to Vizier")
	u.sendNATSMessage("VizierUpdate", reqAnyMsg, vizierID)

	// Wait to receive a response from the vizier that it has received the update message.
	for {
		select {
		case msg := <-subCh:
			v2cMsg := &cvmsgspb.V2CMessage{}
			err := proto.Unmarshal(msg.Data, v2cMsg)
			if err != nil {
				return nil, err
			}
			return v2cMsg, nil
		case <-ctx.Done():
			if ctx.Err() != nil {
				// We never received a response back.
				return nil, errors.New("Vizier did not send response")
			}
		}
	}
}

func (u *Updater) sendNATSMessage(topic string, msg *types.Any, vizierID uuid.UUID) {
	wrappedMsg := &cvmsgspb.C2VMessage{
		VizierID: vizierID.String(),
		Msg:      msg,
	}

	b, err := wrappedMsg.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not marshal message to bytes")
		return
	}
	topic = vzshard.C2VTopic(topic, vizierID)
	log.WithField("topic", topic).Info("Sending message")
	err = u.nc.Publish(topic, b)

	if err != nil {
		log.WithError(err).Error("Could not publish message to nats")
	}
}

// VersionUpToDate checks if the given version string is up to date with the current vizier version.
func (u *Updater) VersionUpToDate(version string) bool {
	// TODO(michelle): Implement.
	return false
}

// AddToUpdateQueue queues the given Vizier for an update to the latest version.
func (u *Updater) AddToUpdateQueue(vizierID uuid.UUID) {
	// TODO(michelle): Implement.
}
