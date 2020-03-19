package vizier

import (
	"context"
	"errors"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"time"

	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
	"pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

const (
	dialTimeout    = 5 * time.Second
	requestTimeout = 1 * time.Second
)

// Contains interface to a running Vizier.

// Connector is an interface to Vizier.
type Connector struct {
	// The ID of the vizier.
	id      uuid.UUID
	conn    *grpc.ClientConn
	qb      querybrokerpb.QueryBrokerServiceClient
	vz      pl_api_vizierpb.VizierServiceClient
	vzIP    string
	vzToken string
}

// NewConnector returns a new connector.
func NewConnector(info *ConnectionInfo) (*Connector, error) {
	c := &Connector{
		id:      info.ID,
		vzIP:    info.URL.Host,
		vzToken: info.Token,
	}

	err := c.connect()
	if err != nil {
		return nil, err
	}

	c.qb = querybrokerpb.NewQueryBrokerServiceClient(c.conn)
	c.vz = pl_api_vizierpb.NewVizierServiceClient(c.conn)
	return c, nil
}

// Connect connects to Vizier (blocking)
func (c *Connector) connect() error {
	addr := c.vzIP
	ctx, cancel := context.WithTimeout(context.Background(), dialTimeout)
	defer cancel()

	// Cancel dial on ctrl-c. Otherwise, it just hangs.
	ch := make(chan os.Signal, 1)
	signal.Notify(ch, os.Interrupt)
	go func() {
		<-ch
		cancel()
	}()
	isInternal := strings.ContainsAny(addr, "cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return err
	}

	dialOpts = append(dialOpts, grpc.WithBlock())
	// Try to dial with a time out (ctrl-c can be used to cancel)
	conn, err := grpc.DialContext(ctx, addr, dialOpts...)
	if err != nil {
		return err
	}
	c.conn = conn

	return nil
}

func (*Connector) nextQueryID() uuid.UUID {
	return uuid.NewV4()
}

// ExecuteScriptStream execute a vizier query as a stream.
func (c *Connector) ExecuteScriptStream(ctx context.Context, q string) (chan *pl_api_vizierpb.ExecuteScriptResponse, error) {
	q = strings.TrimSpace(q)
	if len(q) == 0 {
		return nil, errors.New("input query is empty")
	}
	queryID := c.nextQueryID()
	fmt.Fprintln(os.Stderr, "Executing Script Stream: ", queryID.String())

	reqPB := &pl_api_vizierpb.ExecuteScriptRequest{
		QueryStr:   q,
		FlagValues: nil,
		ClusterID:  c.id.String(),
	}

	ctx, _ = ctxWithCreds(ctx)
	resp, err := c.vz.ExecuteScript(ctx, reqPB)
	if err != nil {
		return nil, err
	}

	results := make(chan *pl_api_vizierpb.ExecuteScriptResponse)
	go func() {
		for {
			select {
			case <-resp.Context().Done():
				return
			case <-ctx.Done():
				return
			default:
				msg, err := resp.Recv()
				if err != nil || msg == nil {
					close(results)
					return
				}
				results <- msg
			}
		}

	}()
	return results, nil
}

// GetAgentInfo returns the agent information.
func (c *Connector) GetAgentInfo() (*querybrokerpb.AgentInfoResponse, error) {
	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	ctx = ctxWithTokenCreds(ctx, c.vzToken)
	defer cancel()

	return c.qb.GetAgentInfo(ctx, &querybrokerpb.AgentInfoRequest{})
}
