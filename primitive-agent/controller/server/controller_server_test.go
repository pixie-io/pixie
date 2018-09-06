package controllerserver

import (
	"io"
	"testing"

	"golang.org/x/net/context"

	"github.com/stretchr/testify/assert"

	pb "pixielabs.ai/pixielabs/primitive-agent/controller/proto"
)

// Implement the interface for Controller_ProcessDataServer.
type ControllerProcessDataServer struct {
	pb.Controller_ProcessDataServer
	calledOnce bool
}

// Implement the interface for Controller_MetadataRelayServer.
type ControllerMetadataRelayServer struct {
	pb.Controller_MetadataRelayServer
	calledOnce   bool
	lastReceived *pb.ControllerMetadataStream
}

type fakeFileWriter struct {
	contents []byte
}

func (m *fakeFileWriter) Write(p []byte) (n int, err error) {
	m.contents = append(m.contents[:], p[:]...)
	return len(p), nil
}

// The first time Recv is called, send some test data. The second time it is called, send EOF.
func (s *ControllerProcessDataServer) Recv() (*pb.AgentDataStream, error) {
	if s.calledOnce == false {
		s.calledOnce = true
		return &pb.AgentDataStream{AgentID: &pb.UUID{Value: "testID"}, Data: "test data"}, nil
	}
	return nil, io.EOF
}

func (s *ControllerProcessDataServer) SendAndClose(resp *pb.AgentDataStreamResponse) error {
	return nil
}

func (s *ControllerMetadataRelayServer) Recv() (*pb.AgentMetadataStream, error) {
	if s.calledOnce == false {
		s.calledOnce = true
		return &pb.AgentMetadataStream{Status: pb.OK}, nil
	}
	return nil, io.EOF
}

func (s *ControllerMetadataRelayServer) Send(resp *pb.ControllerMetadataStream) error {
	s.lastReceived = resp
	return nil
}

func TestRegisterAgent(t *testing.T) {
	assert := assert.New(t)

	s := New(&fakeFileWriter{})

	hostConfig := &pb.HostConfig{KernelVersion: "4.9.93-linuxkit-aufs"}
	req := &pb.RegisterAgentRequest{AgentHostConfig: hostConfig}

	resp, err := s.RegisterAgent(context.Background(), req)

	// RegisterAgent should not throw an error.
	assert.Nil(err)
	assert.Equal(resp.Response, pb.OK, "response code should be OK")
	// RegisterAgent should respond with an ID.
	assert.NotNil(resp.AgentID)
	storedHostConfig, _ := s.agents.Load(resp.AgentID.Value)
	assert.Equal(storedHostConfig.(*pb.HostConfig).KernelVersion, "4.9.93-linuxkit-aufs",
		"RegisterAgent should store correct HostConfig")
}

func TestProcessData(t *testing.T) {
	assert := assert.New(t)

	filewriter := &fakeFileWriter{}
	s := New(filewriter)

	stream := &ControllerProcessDataServer{}
	err := s.ProcessData(stream)

	// ProcessData should not throw an error.
	assert.Nil(err)
	assert.Equal(string(filewriter.contents), "test data", "ProcessData should write correct data to file.")
}

func TestMetadataRelay(t *testing.T) {
	assert := assert.New(t)

	s := New(&fakeFileWriter{})
	stream := &ControllerMetadataRelayServer{}
	err := s.MetadataRelay(stream)

	assert.Nil(err)
	assert.Equal(stream.lastReceived.Status, pb.OK, "MetadataRelay should send OK status.")
}
