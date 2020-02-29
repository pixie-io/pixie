package messageprocessor_test

import (
	"context"
	"fmt"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/sync/errgroup"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/messageprocessor"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	mock_vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb/mock"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/utils"
)

func mustEnvelopeReq(pb proto.Message, topic string) *vzconnpb.CloudConnectRequest {
	pbAsAny, err := types.MarshalAny(pb)
	if err != nil {
		panic(err)
	}

	return &vzconnpb.CloudConnectRequest{
		Topic: topic,
		Msg:   pbAsAny,
	}
}

func mustEnvelopeResp(pb proto.Message, topic string) *vzconnpb.CloudConnectResponse {
	pbAsAny, err := types.MarshalAny(pb)
	if err != nil {
		panic(err)
	}

	return &vzconnpb.CloudConnectResponse{
		Topic: topic,
		Msg:   pbAsAny,
	}
}

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

func TestMessageProcessor(t *testing.T) {
	// TODO(zasgar/michelle): Fix this test.
	t.Skip("This test is currently unstable.")
	u := uuid.NewV4()
	registerReq := &cvmsgspb.RegisterVizierRequest{
		VizierID: utils.ProtoFromUUID(&u),
		JwtKey:   "the-key",
	}

	registerAckOK := &cvmsgspb.RegisterVizierAck{Status: cvmsgspb.ST_OK}
	registerAckFailed := &cvmsgspb.RegisterVizierAck{Status: cvmsgspb.ST_FAILED_NOT_FOUND}

	heartbeatReq := &cvmsgspb.VizierHeartbeat{
		VizierID:       utils.ProtoFromUUID(&u),
		Time:           100,
		SequenceNumber: 1,
	}
	heartbeatOKAck := &cvmsgspb.VizierHeartbeatAck{
		Status:         cvmsgspb.HB_OK,
		Time:           100,
		SequenceNumber: 1,
	}
	sslRequest := &cvmsgspb.VizierSSLCertRequest{
		VizierID: utils.ProtoFromUUID(&u),
	}

	sslResponseOK := &cvmsgspb.VizierSSLCertResponse{
		Key:  "abcd",
		Cert: "efgh",
	}

	tests := []struct {
		name         string
		ins          []*vzconnpb.CloudConnectRequest
		expectInErrs []error

		expectedOuts  []*vzconnpb.CloudConnectResponse
		expectOutErrs []error

		mockExpectations func(client *mock_vzmgrpb.MockVZMgrServiceClient)
	}{
		{
			name: "Successful registration",
			ins: []*vzconnpb.CloudConnectRequest{
				mustEnvelopeReq(registerReq, "register"),
			},
			expectInErrs: []error{nil},

			expectedOuts: []*vzconnpb.CloudConnectResponse{
				mustEnvelopeResp(registerAckOK, "register"),
			},
			expectOutErrs: []error{nil},

			mockExpectations: func(mockVZMgr *mock_vzmgrpb.MockVZMgrServiceClient) {
				mockVZMgr.EXPECT().
					VizierConnected(gomock.Any(), registerReq).
					Return(registerAckOK, nil)
			},
		},
		{
			name: "Register ack failed",
			ins: []*vzconnpb.CloudConnectRequest{
				mustEnvelopeReq(registerReq, "register"),
			},
			expectInErrs: []error{nil},

			expectedOuts: []*vzconnpb.CloudConnectResponse{
				mustEnvelopeResp(registerAckFailed, "register"),
			},
			expectOutErrs: []error{nil},

			mockExpectations: func(mockVZMgr *mock_vzmgrpb.MockVZMgrServiceClient) {
				mockVZMgr.EXPECT().
					VizierConnected(gomock.Any(), registerReq).
					Return(registerAckFailed, nil)
			},
		},
		{
			name: "Heartbeat message after registration",
			ins: []*vzconnpb.CloudConnectRequest{
				mustEnvelopeReq(registerReq, "register"),
				mustEnvelopeReq(heartbeatReq, "heartbeat"),
			},
			expectInErrs: []error{nil, nil},

			expectedOuts: []*vzconnpb.CloudConnectResponse{
				mustEnvelopeResp(registerAckOK, "register"),
				mustEnvelopeResp(heartbeatOKAck, "heartbeat"),
			},
			expectOutErrs: []error{nil, nil},

			mockExpectations: func(mockVZMgr *mock_vzmgrpb.MockVZMgrServiceClient) {

				mockVZMgr.EXPECT().VizierConnected(gomock.Any(), registerReq).
					Return(registerAckOK, nil)
				mockVZMgr.EXPECT().
					HandleVizierHeartbeat(gomock.Any(), heartbeatReq).
					Return(heartbeatOKAck, nil)
			},
		},
		{
			name: "SSL Certs",
			ins: []*vzconnpb.CloudConnectRequest{
				mustEnvelopeReq(registerReq, "register"),
				mustEnvelopeReq(sslRequest, "ssl"),
			},
			expectInErrs: []error{nil, nil},

			expectedOuts: []*vzconnpb.CloudConnectResponse{
				mustEnvelopeResp(registerAckOK, "register"),
				mustEnvelopeResp(sslResponseOK, "ssl"),
			},
			expectOutErrs: []error{nil, nil},

			mockExpectations: func(mockVZMgr *mock_vzmgrpb.MockVZMgrServiceClient) {
				mockVZMgr.EXPECT().VizierConnected(gomock.Any(), registerReq).
					Return(registerAckOK, nil)

				mockVZMgr.EXPECT().GetSSLCerts(gomock.Any(),
					&vzmgrpb.GetSSLCertsRequest{ClusterID: utils.ProtoFromUUID(&u)}).
					Return(&vzmgrpb.GetSSLCertsResponse{Key: "abcd", Cert: "efgh"}, nil)
			},
		},
	}

	for _, tc := range tests {
		testFunc := func(t *testing.T, done chan bool) {
			defer close(done)
			ctrl := gomock.NewController(&goRoutineTestReporter{t: t})
			defer ctrl.Finish()

			mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)
			streamID := uuid.NewV4()
			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()

			m := messageprocessor.NewMessageProcessor(ctx, streamID, mockVZMgr)

			if tc.mockExpectations != nil {
				tc.mockExpectations(mockVZMgr)
			}

			writer := func() error {
				for idx, msg := range tc.ins {
					err := m.ProcessMessage(msg)
					require.Equal(t, tc.expectInErrs[idx], err)
				}
				return nil
			}

			reader := func() error {
				for idx, expectedMsg := range tc.expectedOuts {
					msg, err := m.GetOutgoingMessage()
					require.Equal(t, tc.expectOutErrs[idx], err)
					assert.Equal(t, expectedMsg, msg)
				}
				return nil
			}
			runComplete := make(chan bool)
			go func() {
				m.Run()
				close(runComplete)
			}()

			var g errgroup.Group
			g.Go(reader)
			g.Go(writer)
			err := g.Wait()
			require.Nil(t, err)

			// This should stop the run function.
			m.Stop()
			// Wait for run to complete.
			<-runComplete
		}

		t.Run(tc.name, func(t *testing.T) {
			testDone := make(chan bool)
			timeout := 500 * time.Second
			go testFunc(t, testDone)
			select {
			case <-testDone:
			case <-time.After(timeout):
				t.Fatal("timeout")
			}
		})
	}
}
