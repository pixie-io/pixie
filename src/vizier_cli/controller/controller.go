package controller

import (
	"context"
	"errors"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"time"

	"github.com/olekukonko/tablewriter"
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/utils/proto"
	pb "pixielabs.ai/pixielabs/src/vizier/proto"
)

const defaultVizierAddr = "localhost:40000"
const dialTimeout = 5 * time.Second
const requestTimeout = 1 * time.Second

// Controller is responsible for managing a connection Vizier and making requests.
type Controller struct {
	conn     *grpc.ClientConn
	renderer *TableRenderer
}

// New creates a Controller.
func New() *Controller {
	return &Controller{
		renderer: NewTableRenderer(),
	}
}

func (c *Controller) nextQueryID() uuid.UUID {
	return uuid.NewV4()
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
	fmt.Println("Connected to Vizier")
	c.conn = conn
	return nil
}

// ExecuteQuery will send query execution to Vizier.
func (c *Controller) ExecuteQuery(in string) error {
	in = strings.TrimSpace(in)
	if len(in) == 0 {
		return errors.New("Input query is empty")
	}
	queryID := c.nextQueryID()
	fmt.Println("Executing Query: ", queryID.String())

	err := c.checkConnection()
	if err != nil {
		return err
	}
	reqPb := &pb.QueryRequest{}
	reqPb.QueryID = &utilspb.UUID{}
	reqPb.QueryID.Data = []byte(queryID.String())
	reqPb.QueryStr = in

	cl := pb.NewVizierServiceClient(c.conn)
	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	defer cancel()

	respPb, err := cl.ExecuteQuery(ctx, reqPb)
	if err != nil {
		return err
	}
	fmt.Printf("Got %d Response(s)\n", len(respPb.Responses))
	for _, agentResp := range respPb.Responses {
		agentUUID, err := uuid.FromString(string(agentResp.AgentID.Data))
		if err != nil {
			return err
		}
		fmt.Printf("Agent ID: %s\n", agentUUID)
		bytesProcessed := float64(agentResp.Response.Stats.BytesProcessed)
		execTimeNS := float64(agentResp.Response.Stats.Timing.ExecutionTimeNs)
		for _, table := range agentResp.Response.Tables {
			c.renderer.RenderTable(table)
		}
		fmt.Printf("Compilation Time: %.2f ms\n", float64(agentResp.Response.Stats.Timing.CompilationTimeNs)/1.0E6)
		fmt.Printf("Execution Time: %.2f ms\n", execTimeNS/1.0E6)
		fmt.Printf("Bytes processed: %.2f KB\n", bytesProcessed/1024)
	}
	return nil
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
	table.SetHeader([]string{"AgentID", "Hostname", "Last Heartbeat (seconds)", "State"})
	for _, agentInfo := range respPb.Info {
		id, err := uuid.FromString(string(agentInfo.Info.AgentID.Data))
		if err != nil {
			return err
		}
		hbTime := time.Unix(0, agentInfo.LastHeartbeatNs)
		currentTime := time.Now()
		hbInterval := currentTime.Sub(hbTime).Seconds()
		table.Append([]string{id.String(),
			agentInfo.Info.HostInfo.Hostname,
			fmt.Sprintf("%.2f", hbInterval),
			agentInfo.State.String(),
		})
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
