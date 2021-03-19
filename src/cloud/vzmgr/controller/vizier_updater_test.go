package controller_test

import (
	"sync"
	"testing"

	"github.com/dgrijalva/jwt-go"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	mock_artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb/mock"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/controller"
	"pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func setUpUpdater(t *testing.T) (*controller.Updater, *nats.Conn, *sqlx.DB, *mock_artifacttrackerpb.MockArtifactTrackerClient, func()) {
	viper.Set("jwt_signing_key", "jwtkey")

	mustLoadTestData(db)

	ctrl := gomock.NewController(t)

	nc, natsCleanup := testingutils.MustStartTestNATS(t)

	mockArtifactTrackerClient := mock_artifacttrackerpb.NewMockArtifactTrackerClient(ctrl)
	atReq := &artifacttrackerpb.GetArtifactListRequest{
		ArtifactName: "vizier",
		ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
		Limit:        1,
	}
	mockArtifactTrackerClient.EXPECT().GetArtifactList(
		gomock.Any(), atReq).Return(&versionspb.ArtifactSet{
		Name: "vizier",
		Artifact: []*versionspb.Artifact{&versionspb.Artifact{
			VersionStr: "0.4.1",
		}},
	}, nil)

	updater, _ := controller.NewUpdater(db, mockArtifactTrackerClient, nc)

	cleanup := func() {
		ctrl.Finish()
		natsCleanup()
	}

	return updater, nc, db, mockArtifactTrackerClient, cleanup
}

func TestUpdater_UpdateOrInstallVizier(t *testing.T) {
	updater, nc, _, mockArtifactTrackerClient, cleanup := setUpUpdater(t)
	defer cleanup()

	vizierID, _ := uuid.FromString("123e4567-e89b-12d3-a456-426655440001")

	go nc.Subscribe("c2v.123e4567-e89b-12d3-a456-426655440001.VizierUpdate", func(m *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(m.Data, c2vMsg)
		assert.Nil(t, err)
		resp := &cvmsgspb.UpdateOrInstallVizierRequest{}
		err = types.UnmarshalAny(c2vMsg.Msg, resp)
		assert.Equal(t, "123", resp.Version)
		assert.NotNil(t, resp.Token)
		claims := jwt.MapClaims{}
		_, err = jwt.ParseWithClaims(resp.Token, claims, func(token *jwt.Token) (interface{}, error) {
			return []byte("jwtkey"), nil
		})
		assert.Nil(t, err)
		assert.Equal(t, "cluster", claims["Scopes"].(string))
		// Send response.
		updateResp := &cvmsgspb.UpdateOrInstallVizierResponse{
			UpdateStarted: true,
		}
		respAnyMsg, err := types.MarshalAny(updateResp)
		assert.Nil(t, err)
		wrappedMsg := &cvmsgspb.V2CMessage{
			VizierID: vizierID.String(),
			Msg:      respAnyMsg,
		}

		b, err := wrappedMsg.Marshal()
		assert.Nil(t, err)
		topic := vzshard.V2CTopic("VizierUpdateResponse", vizierID)
		err = nc.Publish(topic, b)
		assert.Nil(t, err)
	})

	mockArtifactTrackerClient.EXPECT().GetDownloadLink(
		gomock.Any(), &artifacttrackerpb.GetDownloadLinkRequest{
			ArtifactName: "vizier",
			VersionStr:   "123",
			ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
		}).Return(&artifacttrackerpb.GetDownloadLinkResponse{}, nil)

	_, err := updater.UpdateOrInstallVizier(vizierID, "123", false)
	assert.Nil(t, err)
}

func TestUpdater_VersionUpToDate(t *testing.T) {
	updater, _, _, _, cleanup := setUpUpdater(t)
	defer cleanup()

	assert.True(t, updater.VersionUpToDate("0.4.1"))
	assert.True(t, updater.VersionUpToDate("0.4.2-pre-rc1"))
	assert.False(t, updater.VersionUpToDate("0.3.1"))
	assert.True(t, updater.VersionUpToDate("0.0.0-dev+Modified.0000000.19700101000000.0"))
}

func TestUpdater_AddToUpdateQueue(t *testing.T) {
	updater, _, _, _, cleanup := setUpUpdater(t)
	defer cleanup()

	id1 := uuid.Must(uuid.NewV4())
	id2 := uuid.Must(uuid.NewV4())

	assert.True(t, updater.AddToUpdateQueue(id1))
	assert.True(t, updater.AddToUpdateQueue(id2))
	assert.False(t, updater.AddToUpdateQueue(id1))
	assert.False(t, updater.AddToUpdateQueue(id2))
}

func TestUpdater_ProcessUpdateQueue(t *testing.T) {
	updater, nc, _, _, cleanup := setUpUpdater(t)
	defer cleanup()
	vizierID, _ := uuid.FromString("123e4567-e89b-12d3-a456-426655440001")

	assert.True(t, updater.AddToUpdateQueue(vizierID))

	var wg sync.WaitGroup
	wg.Add(1)
	defer func() {
		wg.Wait()
		updater.Stop()
	}()

	go nc.Subscribe("c2v.123e4567-e89b-12d3-a456-426655440001.VizierUpdate", func(m *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(m.Data, c2vMsg)
		assert.Nil(t, err)
		resp := &cvmsgspb.UpdateOrInstallVizierRequest{}
		err = types.UnmarshalAny(c2vMsg.Msg, resp)
		assert.Equal(t, "0.4.1", resp.Version)
		assert.NotNil(t, resp.Token)
		claims := jwt.MapClaims{}
		_, err = jwt.ParseWithClaims(resp.Token, claims, func(token *jwt.Token) (interface{}, error) {
			return []byte("jwtkey"), nil
		})
		assert.Nil(t, err)
		assert.Equal(t, "cluster", claims["Scopes"].(string))
		// Send response.
		updateResp := &cvmsgspb.UpdateOrInstallVizierResponse{
			UpdateStarted: true,
		}
		respAnyMsg, err := types.MarshalAny(updateResp)
		assert.Nil(t, err)
		wrappedMsg := &cvmsgspb.V2CMessage{
			VizierID: vizierID.String(),
			Msg:      respAnyMsg,
		}

		b, err := wrappedMsg.Marshal()
		assert.Nil(t, err)
		topic := vzshard.V2CTopic("VizierUpdateResponse", vizierID)
		err = nc.Publish(topic, b)
		assert.Nil(t, err)
		wg.Done()
	})

	go updater.ProcessUpdateQueue()
}
