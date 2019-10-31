package logwriter_test

import (
	"testing"
	"time"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/cloud_connectorpb"
	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/cloud_connectorpb/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/logwriter"
)

func TestLogwriter_MaxInterval(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	client := mock_cloud_connectorpb.NewMockCloudConnectorServiceClient(ctrl)
	mockStream := mock_cloud_connectorpb.NewMockCloudConnectorService_TransferLogClient(ctrl)
	client.EXPECT().TransferLog(gomock.Any()).Return(mockStream, nil)
	pod := "pod-name-123"
	svc := "svc-name"
	writer := logwriter.NewCloudLogWriter(client, "localhost:1234", pod, svc, 100, 0*time.Nanosecond)

	expectedMsg := &cloud_connectorpb.LogMessage{
		Pod: pod,
		Svc: svc,
		Log: "A log message",
	}

	mockStream.EXPECT().Send(&cloud_connectorpb.TransferLogRequest{
		BatchedLogs: []*cloud_connectorpb.LogMessage{expectedMsg},
	}).Return(nil)

	bytes, err := writer.Write([]byte(expectedMsg.Log))
	assert.Equal(t, bytes, len(expectedMsg.Log))
	assert.Equal(t, err, nil)

	writer.Close()
}

func TestLogwriter_MaxQueued(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	client := mock_cloud_connectorpb.NewMockCloudConnectorServiceClient(ctrl)
	mockStream := mock_cloud_connectorpb.NewMockCloudConnectorService_TransferLogClient(ctrl)
	client.EXPECT().TransferLog(gomock.Any()).Return(mockStream, nil)
	pod := "pod-name-123"
	svc := "svc-name"
	writer := logwriter.NewCloudLogWriter(client, "localhost:1234", pod, svc, 2, 100*time.Hour)

	batched := []*cloud_connectorpb.LogMessage{
		&cloud_connectorpb.LogMessage{
			Pod: pod,
			Svc: svc,
			Log: "A log message",
		},
		&cloud_connectorpb.LogMessage{
			Pod: pod,
			Svc: svc,
			Log: "Another log message",
		},
	}

	mockStream.EXPECT().Send(&cloud_connectorpb.TransferLogRequest{
		BatchedLogs: batched,
	}).Return(nil)

	for _, msg := range batched {
		bytes, err := writer.Write([]byte(msg.Log))
		assert.Equal(t, bytes, len(msg.Log))
		assert.Equal(t, err, nil)
	}

	writer.Close()
}

func panicFunc(writer *logwriter.CloudLogWriter) {
	defer func() {
		if r := recover(); r != nil {
			writer.Close()
		}
	}()

	panic("Panicking")
}

func TestLogwriter_CloseAfterFatal(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	client := mock_cloud_connectorpb.NewMockCloudConnectorServiceClient(ctrl)
	mockStream := mock_cloud_connectorpb.NewMockCloudConnectorService_TransferLogClient(ctrl)
	client.EXPECT().TransferLog(gomock.Any()).Return(mockStream, nil)
	pod := "pod-name-123"
	svc := "svc-name"
	writer := logwriter.NewCloudLogWriter(client, "localhost:1234", pod, svc, 100, 100*time.Hour)

	expectedMsg := &cloud_connectorpb.LogMessage{
		Pod: pod,
		Svc: svc,
		Log: "A log message",
	}

	mockStream.EXPECT().Send(&cloud_connectorpb.TransferLogRequest{
		BatchedLogs: []*cloud_connectorpb.LogMessage{expectedMsg},
	}).Return(nil)

	bytes, err := writer.Write([]byte(expectedMsg.Log))
	assert.Equal(t, bytes, len(expectedMsg.Log))
	assert.Equal(t, err, nil)

	panicFunc(writer)
}
