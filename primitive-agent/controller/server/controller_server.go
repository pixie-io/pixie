package controllerserver

import (
	"io"
	"sync"

	"golang.org/x/net/context"

	"github.com/google/uuid"

	pb "pixielabs.ai/pixielabs/primitive-agent/controller/proto"
)

var mutex = &sync.Mutex{}

// New creates and initializes controllerServer.
func New(file io.Writer) *ControllerServer {
	return &ControllerServer{outputFile: file}
}

// ControllerServer handles requests from the agents.
type ControllerServer struct {
	// ControllerServer stores the host configs for each agent in memory.
	agents sync.Map
	// ControllerServer writes agent data to outputFile.
	outputFile io.Writer
}

// RegisterAgent registers agents by receiving requests with the agent's config. This config is saved and the agent
// is assigned a unique id.
func (s *ControllerServer) RegisterAgent(ctx context.Context, req *pb.RegisterAgentRequest) (*pb.RegisterAgentResponse, error) {
	id := uuid.New().String()
	s.agents.Store(id, req.AgentHostConfig)
	return &pb.RegisterAgentResponse{Response: pb.OK, AgentID: &pb.UUID{Value: id}}, nil
}

// ProcessData receives streaming data from the agent. The controller saves this data in a file under the agent's id.
func (s *ControllerServer) ProcessData(stream pb.Controller_ProcessDataServer) error {
	for {
		dataStream, err := stream.Recv()
		if err == io.EOF {
			return stream.SendAndClose(&pb.AgentDataStreamResponse{Response: pb.OK})
		} else if err != nil {
			return err
		}

		mutex.Lock()
		s.outputFile.Write([]byte(dataStream.Data))
		mutex.Unlock()
	}
}
