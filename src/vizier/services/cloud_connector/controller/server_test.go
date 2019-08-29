package controllers_test

import (
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/cloudpb"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	mock_vzconnpb "pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb/mock"
	"pixielabs.ai/pixielabs/src/utils"
	controllers "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/controller"
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

	server := controllers.NewServer(vizierUUID, "test-jwt", mockVZConn)
	server.StartStream()
}
