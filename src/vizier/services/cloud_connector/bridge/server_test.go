package bridge_test

import (
	"context"
	"fmt"
	"io"
	"net"
	"sync"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc"
	"google.golang.org/grpc/test/bufconn"
	"pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb"
	mock_certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/cloud_connector/bridge"
)

const bufSize = 1024 * 1024
const testKey = "my key value"
const testCert = "my cert value"

type FakeVZConnServer struct {
	quitCh chan bool
	msgQ   []*vzconnpb.V2CBridgeMessage
	wg     *sync.WaitGroup
	t      *testing.T
}

func marshalAndSend(srv vzconnpb.VZConnService_NATSBridgeServer, topic string, msg proto.Message) error {
	var respAsAny *types.Any
	var err error
	if respAsAny, err = types.MarshalAny(msg); err != nil {
		return err
	}
	outMsg := &vzconnpb.C2VBridgeMessage{
		Topic: topic,
		Msg:   respAsAny,
	}
	return srv.Send(outMsg)
}

func handleMsg(srv vzconnpb.VZConnService_NATSBridgeServer, msg *vzconnpb.V2CBridgeMessage) error {
	if msg.Topic == "register" {
		return marshalAndSend(srv, "registerAck", &cvmsgspb.RegisterVizierAck{Status: cvmsgspb.ST_OK})
	}

	if msg.Topic == "ssl" {
		msg := &cvmsgspb.VizierSSLCertResponse{
			Key:  testKey,
			Cert: testCert,
		}
		return marshalAndSend(srv, "sslResp", msg)
	}

	if msg.Topic == "randomtopic" {
		return nil
	}

	return fmt.Errorf("Got unknown topic %s", msg.Topic)
}

// NATSBridge is the endpoint that all viziers connect to.
func (fs *FakeVZConnServer) NATSBridge(srv vzconnpb.VZConnService_NATSBridgeServer) error {
	for {
		select {
		case <-srv.Context().Done():
			return nil
		case <-fs.quitCh:
			return nil
		default:
			msg, err := srv.Recv()
			if err != nil && err == io.EOF {
				// stream closed.
				return nil
			}
			if err != nil {
				return err
			}
			// Ignore heartbeats
			if msg.Topic != bridge.HeartbeatTopic {
				fs.msgQ = append(fs.msgQ, msg)
				err = handleMsg(srv, msg)
				if err != nil {
					fs.t.Errorf("Error marshalling: %+v", err)
					return err
				}
				fs.wg.Done()
			}
		}
	}
}

func newFakeVZConnServer(wg *sync.WaitGroup, t *testing.T) *FakeVZConnServer {
	return &FakeVZConnServer{
		quitCh: make(chan bool),
		wg:     wg,
		t:      t,
	}
}

type FakeVZInfo struct {
	externalAddr string
	port         int32
}

func makeFakeVZInfo(externalAddr string, port int32) bridge.VizierInfo {
	return &FakeVZInfo{
		externalAddr: externalAddr,
		port:         port,
	}
}

func (f *FakeVZInfo) GetAddress() (string, int32, error) {
	return f.externalAddr, f.port, nil
}

type testState struct {
	vzServer    *FakeVZConnServer
	vzClient    vzconnpb.VZConnServiceClient
	mockCertMgr *mock_certmgrpb.MockCertMgrServiceClient
	nats        *nats.Conn
	vzID        uuid.UUID
	jwt         string
	wg          *sync.WaitGroup
	lis         *bufconn.Listener
	s           *grpc.Server
}

func createDialer(lis *bufconn.Listener) func(string, time.Duration) (net.Conn, error) {
	return func(str string, duration time.Duration) (conn net.Conn, e error) {
		return lis.Dial()
	}
}

func makeTestState(t *testing.T, ctrl *gomock.Controller) (*testState, func(t *testing.T)) {
	lis := bufconn.Listen(bufSize)
	s := grpc.NewServer()
	wg := &sync.WaitGroup{}
	vs := newFakeVZConnServer(wg, t)
	vzconnpb.RegisterVZConnServiceServer(s, vs)
	go func() {
		if err := s.Serve(lis); err != nil {
			t.Fatalf("Server exited with error: %v\n", err)
		}
	}()

	ctx := context.Background()
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithDialer(createDialer(lis)), grpc.WithInsecure())
	if err != nil {
		t.Fatalf("Got an error during GRPC setup: %+v", err)
	}
	vc := vzconnpb.NewVZConnServiceClient(conn)
	natsPort, natsCleanup := testingutils.StartNATS(t)
	nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
	if err != nil {
		t.Fatalf("Got an error during NATS setup: %+v", err)
	}

	cleanupFunc := func(t *testing.T) {
		natsCleanup()
		conn.Close()

	}

	u, err := uuid.FromString("31285cdd-1de9-4ab1-ae6a-0ba08c8c676c")
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	return &testState{
		vzID:        u,
		vzServer:    vs,
		vzClient:    vc,
		mockCertMgr: mock_certmgrpb.NewMockCertMgrServiceClient(ctrl),
		nats:        nc,
		jwt:         testingutils.GenerateTestJWTToken(t, "jwt-key"),
		wg:          wg,
		lis:         lis,
	}, cleanupFunc
}

func TestNATSGRPCBridgeTest_CorrectRegistrationFlow(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	ts, cleanup := makeTestState(t, ctrl)
	defer cleanup(t)

	sessionID := time.Now().UnixNano()
	b := bridge.New(ts.vzID, ts.jwt, sessionID, ts.vzClient, ts.mockCertMgr, makeFakeVZInfo("foobar", 123), ts.nats)
	defer b.Stop()
	go b.RunStream()

	expectedV2CMsgs := 2
	ts.wg.Add(expectedV2CMsgs)
	ts.wg.Add(1) // add an extra one for the cert manager reply

	ts.mockCertMgr.EXPECT().
		UpdateCerts(gomock.Any(), gomock.Any()).
		DoAndReturn(func(ctx context.Context, req *certmgrpb.UpdateCertsRequest) (*certmgrpb.UpdateCertsResponse, error) {
			ts.wg.Done()
			assert.Equal(t, req.Key, testKey)
			assert.Equal(t, req.Cert, testCert)
			return &certmgrpb.UpdateCertsResponse{
				OK: true,
			}, nil
		})

	ts.wg.Wait()
	assert.Equal(t, expectedV2CMsgs, len(ts.vzServer.msgQ))

	register := ts.vzServer.msgQ[0]
	ssl := ts.vzServer.msgQ[1]

	// Check the metadata
	assert.Equal(t, "register", register.Topic)
	assert.Equal(t, sessionID, register.SessionId)
	assert.Equal(t, "ssl", ssl.Topic)
	assert.Equal(t, sessionID, ssl.SessionId)

	// Check the contents
	registerMsg := &cvmsgspb.RegisterVizierRequest{}
	err := types.UnmarshalAny(register.Msg, registerMsg)
	if err != nil {
		t.Fatalf("Could not unmarshal: %+v", err)
	}
	assert.Equal(t, string(registerMsg.VizierID.Data), ts.vzID.String())
	assert.Equal(t, registerMsg.JwtKey, ts.jwt)
	assert.Equal(t, registerMsg.Address, "foobar")

	// Check the contents
	sslMsg := &cvmsgspb.VizierSSLCertRequest{}
	err = types.UnmarshalAny(ssl.Msg, sslMsg)
	if err != nil {
		t.Fatalf("Could not unmarshal: %+v", err)
	}
	assert.Equal(t, string(registerMsg.VizierID.Data), ts.vzID.String())
}

// Test a message that comes from our NATS queue (and should end up sent to the VZConn)
func TestNATSGRPCBridgeTest_TestOutboundNATSMessage(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	ts, cleanup := makeTestState(t, ctrl)
	defer cleanup(t)

	sessionID := time.Now().UnixNano()
	b := bridge.New(ts.vzID, ts.jwt, sessionID, ts.vzClient, ts.mockCertMgr, makeFakeVZInfo("foobar", 123), ts.nats)
	defer b.Stop()
	go b.RunStream()

	expectedV2CMsgs := 3
	ts.wg.Add(expectedV2CMsgs)
	ts.wg.Add(1) // add an extra one for the cert manager reply

	logmsg := &cvmsgspb.VLogMessage{
		Data: []byte("Foobar"),
	}

	ts.mockCertMgr.EXPECT().
		UpdateCerts(gomock.Any(), gomock.Any()).
		DoAndReturn(func(ctx context.Context, req *certmgrpb.UpdateCertsRequest) (*certmgrpb.UpdateCertsResponse, error) {
			ts.wg.Done() // cert mgr reply

			subany, err := types.MarshalAny(logmsg)
			if err != nil {
				t.Fatal("Error marshalling msg: %+v", err)
			}
			msg := &cvmsgspb.V2CMessage{
				VizierID:  ts.vzID.String(),
				SessionId: sessionID,
				Msg:       subany,
			}
			b, err := msg.Marshal()
			if err != nil {
				t.Fatalf("Error marshalling msg: %+v", err)
			}
			inMsg := &nats.Msg{Subject: "v2c.randomtopic", Data: b}
			err = ts.nats.PublishMsg(inMsg)
			if err != nil {
				t.Fatalf("Error publishing NATS msg: %+v", err)
			}
			return &certmgrpb.UpdateCertsResponse{
				OK: true,
			}, nil
		})

	ts.wg.Wait()
	assert.Equal(t, expectedV2CMsgs, len(ts.vzServer.msgQ))

	msg := ts.vzServer.msgQ[2]
	assert.Equal(t, "randomtopic", msg.Topic)
	assert.Equal(t, sessionID, msg.SessionId)

	expected := &cvmsgspb.VLogMessage{}
	err := types.UnmarshalAny(msg.Msg, expected)
	if err != nil {
		t.Fatalf("Error Unmarshaling: %+v", err)
	}

	assert.Equal(t, expected, logmsg)
}

// Test a message that is sent by VZConn and should end up in our NATS queue
func TestNATSGRPCBridgeTest_TestInboundNATSMessage(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	ts, cleanup := makeTestState(t, ctrl)
	defer cleanup(t)

	sessionID := time.Now().UnixNano()
	b := bridge.New(ts.vzID, ts.jwt, sessionID, ts.vzClient, ts.mockCertMgr, makeFakeVZInfo("foobar", 123), ts.nats)
	defer b.Stop()

	// Subscribe to NATS
	natsCh := make(chan *nats.Msg)
	natsSub, err := ts.nats.ChanSubscribe("c2v.*", natsCh)
	if err != nil {
		t.Fatalf("Error subscribing to channel: %+v", err)
	}

	var inboundNats *nats.Msg
	ts.wg.Add(1) // For the nats msg
	go func() {
		inboundNats = <-natsCh
		natsSub.Unsubscribe()
		ts.wg.Done()
	}()

	go b.RunStream()

	expectedV2CMsgs := 2
	ts.wg.Add(expectedV2CMsgs)
	ts.wg.Add(1) // add an extra one for the cert manager reply

	ts.mockCertMgr.EXPECT().
		UpdateCerts(gomock.Any(), gomock.Any()).
		DoAndReturn(func(ctx context.Context, req *certmgrpb.UpdateCertsRequest) (*certmgrpb.UpdateCertsResponse, error) {
			ts.wg.Done()
			return &certmgrpb.UpdateCertsResponse{
				OK: true,
			}, nil
		})

	ts.wg.Wait()
	assert.Equal(t, expectedV2CMsgs, len(ts.vzServer.msgQ))
	assert.Equal(t, inboundNats.Subject, "c2v.sslResp")

	// Unmarshal and check the nats message
	var msgAny *types.Any
	expectedMsg := &cvmsgspb.VizierSSLCertResponse{
		Key:  testKey,
		Cert: testCert,
	}
	if msgAny, err = types.MarshalAny(expectedMsg); err != nil {
		t.Fatalf("Error marshaling: %+v", err)
	}
	expectedNats := &cvmsgspb.C2VMessage{
		VizierID: ts.vzID.String(),
		Msg:      msgAny,
	}

	actualNats := &cvmsgspb.C2VMessage{}
	err = actualNats.Unmarshal(inboundNats.Data)
	if err != nil {
		t.Fatalf("Error unmarshaling: %+v", err)
	}
	assert.Equal(t, expectedNats, actualNats)
}
