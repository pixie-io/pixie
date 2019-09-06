package controller

import (
	"context"
	"errors"
	"fmt"
	"testing"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"github.com/magiconair/properties/assert"

	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"

	mock_controller "pixielabs.ai/pixielabs/src/cloud/vzconn/controller/mock"

	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/require"
	mock_vzconnpb "pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb/mock"
)

// goRoutineTestReporter is needed because gomock will hang if it has an expectation error in a goroutine.
type goRoutineTestReporter struct {
	t *testing.T
}

func (g *goRoutineTestReporter) Errorf(format string, args ...interface{}) {
	g.t.Errorf(format, args...)
}

func (g *goRoutineTestReporter) Fatalf(format string, args ...interface{}) {
	panic(fmt.Sprintf(format, args...))
}

func TestServer_CloudConnect_MessageRouting(t *testing.T) {
	ctrl := gomock.NewController(&goRoutineTestReporter{t: t})
	defer ctrl.Finish()

	stream := mock_vzconnpb.NewMockVZConnService_CloudConnectServer(ctrl)
	mockProcessor := mock_controller.NewMockmessageProcessor(ctrl)
	// Setup the context for the request and return it when called.
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	stream.EXPECT().Context().Return(ctx)

	streamID := uuid.NewV4()

	mockProcessor.EXPECT().
		Stop().Return().MinTimes(1)
	mockProcessor.EXPECT().
		ProcessMessage(&vzconnpb.CloudConnectRequest{}).Return(nil)

	// Send Stream.
	gomock.InOrder(
		mockProcessor.EXPECT().
			GetOutgoingMessage().Return(&vzconnpb.CloudConnectResponse{}, nil),
		// nil, nil means stream termination.
		mockProcessor.EXPECT().
			GetOutgoingMessage().Return(nil, nil).AnyTimes(),
	)

	// Success.
	mockProcessor.EXPECT().
		Run().Return(nil)
	stream.EXPECT().Send(&vzconnpb.CloudConnectResponse{}).Return(nil)

	// Receive stream.
	gomock.InOrder(
		stream.EXPECT().Recv().
			Return(&vzconnpb.CloudConnectRequest{}, nil),
		stream.EXPECT().
			Recv().Return(nil, nil).AnyTimes(),
	)

	s := NewServer(nil)
	err := s.CloudConnectWithProcessor(streamID, mockProcessor, stream)
	require.Nil(t, err)
}

func TestServer_CloudConnect_Cancelled(t *testing.T) {
	ctrl := gomock.NewController(&goRoutineTestReporter{t: t})
	defer ctrl.Finish()

	stream := mock_vzconnpb.NewMockVZConnService_CloudConnectServer(ctrl)
	mockProcessor := mock_controller.NewMockmessageProcessor(ctrl)
	// Setup the context for the request and return it when called.
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	stream.EXPECT().Context().Return(ctx)

	streamID := uuid.NewV4()

	mockProcessor.EXPECT().
		Stop().Return().MinTimes(1)
	mockProcessor.EXPECT().
		GetOutgoingMessage().Return(nil, nil).MaxTimes(1)
	mockProcessor.EXPECT().
		Run().Return(nil).MaxTimes(1)
	stream.EXPECT().Recv().
		Return(nil, nil).MaxTimes(1)

	s := NewServer(nil)
	err := s.CloudConnectWithProcessor(streamID, mockProcessor, stream)
	assert.Equal(t, status.Code(err), codes.Canceled)
}

func TestServer_CloudConnect_MessageRouting_FailedProcessMessage(t *testing.T) {
	ctrl := gomock.NewController(&goRoutineTestReporter{t: t})
	defer ctrl.Finish()

	stream := mock_vzconnpb.NewMockVZConnService_CloudConnectServer(ctrl)
	mockProcessor := mock_controller.NewMockmessageProcessor(ctrl)
	// Setup the context for the request and return it when called.
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	stream.EXPECT().Context().Return(ctx)

	streamID := uuid.NewV4()

	mockProcessor.EXPECT().
		Stop().Return().MinTimes(1)
	mockProcessor.EXPECT().
		ProcessMessage(&vzconnpb.CloudConnectRequest{}).Return(errors.New("Failed"))
	// Send Stream.
	mockProcessor.EXPECT().
		GetOutgoingMessage().Return(nil, nil)
	// Success.
	mockProcessor.EXPECT().
		Run().Return(nil)

	// Receive stream.
	gomock.InOrder(
		stream.EXPECT().Recv().
			Return(&vzconnpb.CloudConnectRequest{}, nil),
		stream.EXPECT().
			Recv().Return(nil, nil).AnyTimes(),
	)

	s := NewServer(nil)
	err := s.CloudConnectWithProcessor(streamID, mockProcessor, stream)
	require.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.Unknown)
}
