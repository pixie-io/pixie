package controllers_test

import (
	"io"
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
	certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb"
	mock_certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb/mock"
	cloud_connectorpb "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/cloud_connectorpb"
	mock_cloud_connectorpb "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/cloud_connectorpb/mock"
	controllers "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/controller"
	mock_controller "pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/controller/mock"
)

func TestServer_Register(t *testing.T) {
	vizierID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	vizierUUID, err := uuid.FromString(vizierID)
	assert.Nil(t, err)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockVZConn := mock_vzconnpb.NewMockVZConnServiceClient(ctrl)
	mockCertMgr := mock_certmgrpb.NewMockCertMgrServiceClient(ctrl)
	mockStream := mock_vzconnpb.NewMockVZConnService_CloudConnectClient(ctrl)

	regReq := &cloudpb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUID(&vizierUUID),
		JwtKey:   "test-jwt",
		Address:  "https://127.0.0.1",
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
	mockVzInfo.EXPECT().GetAddress().Return("https://127.0.0.1", int32(123), nil)

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	server := controllers.NewServerWithClock(vizierUUID, "test-jwt", mockVZConn, mockCertMgr, mockVzInfo, clock)
	err = server.RegisterVizier(mockStream)
	assert.Nil(t, err)
}

func TestServer_HandleHeartbeat(t *testing.T) {
	vizierID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	vizierUUID, err := uuid.FromString(vizierID)
	assert.Nil(t, err)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockVZConn := mock_vzconnpb.NewMockVZConnServiceClient(ctrl)
	mockCertMgr := mock_certmgrpb.NewMockCertMgrServiceClient(ctrl)
	mockStream := mock_vzconnpb.NewMockVZConnService_CloudConnectClient(ctrl)

	hbReq := &cloudpb.VizierHeartbeat{
		VizierID:       utils.ProtoFromUUID(&vizierUUID),
		Time:           10,
		SequenceNumber: 0,
		Address:        "https://127.0.0.1",
		Port:           int32(123),
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
	mockVzInfo.EXPECT().GetAddress().Return("https://127.0.0.1", int32(123), nil)

	clock := testingutils.NewTestClock(time.Unix(10, 0))
	server := controllers.NewServerWithClock(vizierUUID, "test-jwt", mockVZConn, mockCertMgr, mockVzInfo, clock)
	err = server.HandleHeartbeat(mockStream)
	assert.Nil(t, err)
}

func TestServer_HandleHeartbeat_EOFError(t *testing.T) {
	vizierID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	vizierUUID, err := uuid.FromString(vizierID)
	assert.Nil(t, err)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockVZConn := mock_vzconnpb.NewMockVZConnServiceClient(ctrl)
	mockCertMgr := mock_certmgrpb.NewMockCertMgrServiceClient(ctrl)
	mockStream := mock_vzconnpb.NewMockVZConnService_CloudConnectClient(ctrl)

	hbReq := &cloudpb.VizierHeartbeat{
		VizierID:       utils.ProtoFromUUID(&vizierUUID),
		Time:           10,
		SequenceNumber: 0,
		Address:        "https://127.0.0.1",
		Port:           int32(123),
	}
	anyMsg, err := types.MarshalAny(hbReq)
	assert.Nil(t, err)
	wrappedReq := &vzconnpb.CloudConnectRequest{
		Topic: "heartbeat",
		Msg:   anyMsg,
	}

	mockStream.EXPECT().Send(wrappedReq).Return(nil)
	mockStream.EXPECT().Recv().Return(nil, io.EOF)

	mockVzInfo := mock_controller.NewMockVizierInfo(ctrl)
	mockVzInfo.EXPECT().GetAddress().Return("https://127.0.0.1", int32(123), nil)

	clock := testingutils.NewTestClock(time.Unix(10, 0))
	server := controllers.NewServerWithClock(vizierUUID, "test-jwt", mockVZConn, mockCertMgr, mockVzInfo, clock)
	err = server.HandleHeartbeat(mockStream)
	assert.NotNil(t, err)
}

func TestServer_RequestAndHandleSSLCerts(t *testing.T) {
	vizierID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	vizierUUID, err := uuid.FromString(vizierID)
	assert.Nil(t, err)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockVZConn := mock_vzconnpb.NewMockVZConnServiceClient(ctrl)
	mockCertMgr := mock_certmgrpb.NewMockCertMgrServiceClient(ctrl)

	mockStream := mock_vzconnpb.NewMockVZConnService_CloudConnectClient(ctrl)
	mockStream.EXPECT().Context().Return(nil)

	req := &cloudpb.VizierSSLCertRequest{
		VizierID: utils.ProtoFromUUID(&vizierUUID),
	}

	anyMsg, err := types.MarshalAny(req)
	assert.Nil(t, err)
	wrappedReq := &vzconnpb.CloudConnectRequest{
		Topic: "ssl",
		Msg:   anyMsg,
	}

	mockStream.EXPECT().Send(wrappedReq).Return(nil)

	resp := &cloudpb.VizierSSLCertResponse{
		Key:  "abcd",
		Cert: "efgh",
	}
	respAny, err := types.MarshalAny(resp)
	assert.Nil(t, err)
	wrappedResp := &vzconnpb.CloudConnectResponse{
		Topic: "ssl",
		Msg:   respAny,
	}
	mockStream.EXPECT().Recv().Return(wrappedResp, nil)

	mockVzInfo := mock_controller.NewMockVizierInfo(ctrl)

	mockCertMgr.EXPECT().UpdateCerts(gomock.Any(), &certmgrpb.UpdateCertsRequest{
		Key:  "abcd",
		Cert: "efgh",
	}).Return(&certmgrpb.UpdateCertsResponse{OK: true}, nil)

	clock := testingutils.NewTestClock(time.Unix(10, 0))
	server := controllers.NewServerWithClock(vizierUUID, "test-jwt", mockVZConn, mockCertMgr, mockVzInfo, clock)
	server.RequestAndHandleSSLCerts(mockStream)
}

func TestServer_HandleLog(t *testing.T) {
	vizierID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	vizierUUID, err := uuid.FromString(vizierID)
	assert.Nil(t, err)

	logMessage := &cloud_connectorpb.TransferLogRequest{
		BatchedLogs: []*cloud_connectorpb.LogMessage{
			&cloud_connectorpb.LogMessage{
				Pod: "bar",
				Svc: "xyz",
				Log: "log msg",
			},
		},
	}
	anyMsg, err := types.MarshalAny(logMessage)
	assert.Nil(t, err)
	wrappedReq := &vzconnpb.CloudConnectRequest{
		Topic: "logs",
		Msg:   anyMsg,
	}

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockVZConn := mock_vzconnpb.NewMockVZConnServiceClient(ctrl)
	mockCertMgr := mock_certmgrpb.NewMockCertMgrServiceClient(ctrl)

	// Set up streams to vzconn
	mockVZConnStream := mock_vzconnpb.NewMockVZConnService_CloudConnectClient(ctrl)
	// Expect that the VZConn stream will forward the request
	mockVZConnStream.EXPECT().Send(wrappedReq).Return(nil).Times(2)

	mockVzInfo := mock_controller.NewMockVizierInfo(ctrl)
	clock := testingutils.NewTestClock(time.Unix(10, 0))
	server := controllers.NewServerWithClock(vizierUUID, "test-jwt", mockVZConn, mockCertMgr, mockVzInfo, clock)

	// Set up streams from other vizier services to the cloud connector
	mockLogStream := mock_cloud_connectorpb.NewMockCloudConnectorService_TransferLogServer(ctrl)
	mockLogStream.EXPECT().Recv().Return(logMessage, nil).Times(2)
	mockLogStream.EXPECT().Recv().Return(nil, io.EOF)
	mockLogStream.EXPECT().SendAndClose(&cloud_connectorpb.TransferLogResponse{
		Ok: true,
	}).Return(nil)

	finished := make(chan bool)
	go func(finished chan bool) {
		err := server.DoLogTransfer(mockVZConnStream)
		assert.Nil(t, err)
		finished <- true
	}(finished)

	err = server.TransferLog(mockLogStream)
	assert.Nil(t, err)
	server.Stop()
	<-finished
}
