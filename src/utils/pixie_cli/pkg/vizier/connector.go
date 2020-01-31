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
	plannerpb "pixielabs.ai/pixielabs/src/carnot/compiler/plannerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
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

// ExecuteScript executes the specified script.
func (c *Connector) ExecuteScript(q string) (*querybrokerpb.VizierQueryResponse, error) {
	q = strings.TrimSpace(q)
	if len(q) == 0 {
		return nil, errors.New("input query is empty")
	}
	queryID := c.nextQueryID()
	fmt.Fprintln(os.Stderr, "Executing Script: ", queryID.String())

	reqPb := &plannerpb.QueryRequest{}
	reqPb.QueryStr = q

	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	ctx = ctxWithTokenCreds(ctx, c.vzToken)
	defer cancel()

	return c.qb.ExecuteQuery(ctx, reqPb)
}

// GetAgentInfo returns the agent information.
func (c *Connector) GetAgentInfo() (*querybrokerpb.AgentInfoResponse, error) {
	ctx, cancel := context.WithTimeout(context.Background(), requestTimeout)
	ctx = ctxWithTokenCreds(ctx, c.vzToken)
	defer cancel()

	return c.qb.GetAgentInfo(ctx, &querybrokerpb.AgentInfoRequest{})
}
