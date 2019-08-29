package controllers_test

import (
	"testing"
	"time"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/cloudpb"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	mock_vzconnpb "pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb/mock"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	controllers "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/controller"
	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/controller/mock"
)

func TestServer_Register(t *testing.T) {
	vizierID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	vizierUUID, err := uuid.FromString(vizierID)
	assert.Nil(t, err)

	ctrl := gomock.NewController(t)
	mockVZConn := mock_vzconnpb.NewMockVZConnServiceClient(ctrl)

	mockStream := mock_vzconnpb.NewMockVZConnService_CloudConnectClient(ctrl)

	mockVZConn.EXPECT().CloudConnect(gomock.Any()).
		Return(mockStream, nil)

	regReq := &cloudpb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUID(&vizierUUID),
		JwtKey:   "test-jwt",
	}
	anyMsg, err := types.MarshalAny(regReq)
	assert.Nil(t, err)
	wrappedReq := &vzconnpb.CloudConnectRequest{
		Topic: "register",
		Msg:   anyMsg,
	}

	mockStream.EXPECT().Send(wrappedReq).Return(nil)

	regResp := &cloudpb.RegisterVizierAck{
		Status: cloudpb.ST_OK,
	}
	regRespAny, err := types.MarshalAny(regResp)
	assert.Nil(t, err)
	wrappedResp := &vzconnpb.CloudConnectResponse{
		Topic: "register",
		Msg:   regRespAny,
	}
	mockStream.EXPECT().Recv().Return(wrappedResp, nil)

	mockVzInfo := mock_controller.NewMockVizierInfo(ctrl)

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	server := controllers.NewServerWithClock(vizierUUID, "test-jwt", mockVZConn, mockVzInfo, clock)
	err = server.RegisterVizier(mockStream)
	assert.Nil(t, err)
}

func TestServer_HandleHeartbeat(t *testing.T) {
	vizierID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	vizierUUID, err := uuid.FromString(vizierID)
	assert.Nil(t, err)

	ctrl := gomock.NewController(t)
	mockVZConn := mock_vzconnpb.NewMockVZConnServiceClient(ctrl)

	mockStream := mock_vzconnpb.NewMockVZConnService_CloudConnectClient(ctrl)

	mockVZConn.EXPECT().CloudConnect(gomock.Any()).
		Return(mockStream, nil)

	hbReq := &cloudpb.VizierHeartbeat{
		VizierID:       utils.ProtoFromUUID(&vizierUUID),
		Time:           10,
		SequenceNumber: 0,
		Address:        "https://127.0.0.1",
	}
	anyMsg, err := types.MarshalAny(hbReq)
	assert.Nil(t, err)
	wrappedReq := &vzconnpb.CloudConnectRequest{
		Topic: "heartbeat",
		Msg:   anyMsg,
	}

	mockStream.EXPECT().Send(wrappedReq).Return(nil)

	hbResp := &cloudpb.VizierHeartbeatAck{
		SequenceNumber: 0,
	}
	hbRespAny, err := types.MarshalAny(hbResp)
	assert.Nil(t, err)
	wrappedResp := &vzconnpb.CloudConnectResponse{
		Topic: "hearbeat",
		Msg:   hbRespAny,
	}
	mockStream.EXPECT().Recv().Return(wrappedResp, nil)

	mockVzInfo := mock_controller.NewMockVizierInfo(ctrl)
	mockVzInfo.EXPECT().GetAddress().Return("https://127.0.0.1", nil)

	clock := testingutils.NewTestClock(time.Unix(10, 0))
	server := controllers.NewServerWithClock(vizierUUID, "test-jwt", mockVZConn, mockVzInfo, clock)
	server.HandleHeartbeat(mockStream)
}
