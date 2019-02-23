package controller

import (
	"context"
	"errors"
	"fmt"
	"os"
	"os/signal"
	"time"

	"github.com/google/uuid"
	"github.com/olekukonko/tablewriter"
	"google.golang.org/grpc"
	pb "pixielabs.ai/pixielabs/src/vizier/proto"
)

const defaultVizierAddr = "localhost:40000"
const dialTimeout = 5 * time.Second
const requestTimeout = 1 * time.Second

// Controller is responsible for managing a connection Vizier and making requests.
type Controller struct {
	conn *grpc.ClientConn
}

// New creates a Controller.
func New() *Controller {
	return &Controller{}
}

func (c *Controller) checkConnection() error {
	if c.conn == nil {
		return fmt.Errorf("not connected to Vizier")
	}
	return nil
}

// Connect will connect to Vizier.
func (c *Controller) Connect(addr string) error {
	if addr == "" {
		addr = defaultVizierAddr
	}
	fmt.Printf("Connecting to vizier at: %s\n", addr)

	ctx, cancel := context.WithTimeout(context.Background(), dialTimeout)
	defer cancel()

	// Cancel dial on ctrl-c. Otherwise, it just hangs.
	ch := make(chan os.Signal, 1)
	signal.Notify(ch, os.Interrupt)
	go func() {
		<-ch
		cancel()
	}()

	// Try to dial with a time out (ctrl-c can be used to cancel)
	conn, err := grpc.DialContext(ctx, addr, grpc.WithBlock(), grpc.WithInsecure())
	if err != nil {
		return err
	}
	c.conn = conn
	return nil
}

// ExecuteQuery will send query execution to Vizier.
func (c *Controller) ExecuteQuery(in string) error {
	err := c.checkConnection()
	if err != nil {
		return err
	}
	fmt.Println("Executing Query: ")
	fmt.Println(in)
	return errors.New("Not connected to agent")
}

// PrintAgentInfo will print agents connected to Vizier.
func (c *Controller) PrintAgentInfo() error {
	err := c.checkConnection()
	if err != nil {
		return err
	}
	reqPb := &pb.AgentInfoRequest{}
	cl := pb.NewVizierServiceClient(c.conn)
	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	respPb, err := cl.GetAgentInfo(ctx, reqPb)
	if err != nil {
		return err
	}

	fmt.Printf("Number of agents: %d\n", len(respPb.Info))

	table := tablewriter.NewWriter(os.Stdout)
	table.SetHeader([]string{"AgentID", "Hostname"})
	for _, agentInfo := range respPb.Info {
		id, err := uuid.ParseBytes(agentInfo.AgentID.Data)
		if err != nil {
			return err
		}
		table.Append([]string{id.String(), agentInfo.HostInfo.Hostname})
	}
	table.Render()
	return nil
}

// Shutdown will shutdown the connection to Vizier (must be called to prevent socket leak)
func (c *Controller) Shutdown() {
	if c.conn != nil {
		c.conn.Close()
	}
}
