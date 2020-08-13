package controller_test

import (
	"testing"

	"github.com/dgrijalva/jwt-go"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	mock_artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb/mock"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzshard"
	"pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"

	"pixielabs.ai/pixielabs/src/cloud/vzmgr/controller"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestUpdater_UpdateOrInstallVizier(t *testing.T) {
	viper.Set("jwt_signing_key", "jwtkey")

	db, teardown := setupTestDB(t)
	defer teardown()
	loadTestData(t, db)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

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
			VersionStr: "test",
		}},
	}, nil)

	mockArtifactTrackerClient.EXPECT().GetDownloadLink(
		gomock.Any(), &artifacttrackerpb.GetDownloadLinkRequest{
			ArtifactName: "vizier",
			VersionStr:   "123",
			ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
		}).Return(&artifacttrackerpb.GetDownloadLinkResponse{}, nil)

	updater, _ := controller.NewUpdater(db, mockArtifactTrackerClient, nc)
	updater.UpdateOrInstallVizier(vizierID, "123", false)
}
