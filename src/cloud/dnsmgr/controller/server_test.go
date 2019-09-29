package controller_test

import (
	"context"
	"testing"

	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/controller"
	mock_controller "pixielabs.ai/pixielabs/src/cloud/dnsmgr/controller/mock"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
	"pixielabs.ai/pixielabs/src/utils"
)

func TestServer_GetDNSAddress(t *testing.T) {
	ctrl := gomock.NewController(t)
	mockDNS := mock_controller.NewMockDNSService(ctrl)

	s := controller.NewServer(nil, mockDNS)

	clusterID := uuid.NewV4()

	req := &dnsmgrpb.GetDNSAddressRequest{
		ClusterID: utils.ProtoFromUUID(&clusterID),
		IPAddress: "127.0.0.1",
	}

	mockDNS.EXPECT().
		CreateResourceRecord("test", "127.0.0.1", int64(controller.ResourceRecordTTL)).
		Return(nil)

	resp, err := s.GetDNSAddress(context.Background(), req)
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, "test", resp.DNSAddress)
}
