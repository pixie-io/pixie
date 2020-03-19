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
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/utils"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

const (
	dialTimeout = 5 * time.Second
)

// Connector is an interface to Vizier.
type Connector struct {
	// The ID of the vizier.
	id                 uuid.UUID
	conn               *grpc.ClientConn
	vz                 pl_api_vizierpb.VizierServiceClient
	vzToken            string
	passthroughEnabled bool
}

// NewConnector returns a new connector.
func NewConnector(cloudAddr string, vzInfo *cloudapipb.ClusterInfo, conn *ConnectionInfo) (*Connector, error) {
	c := &Connector{
		id: utils.UUIDFromProtoOrNil(vzInfo.ID),
	}

	if vzInfo.Config != nil {
		c.passthroughEnabled = vzInfo.Config.PassthroughEnabled
	}

	var err error
	if !c.passthroughEnabled {
		// We need to store the token to talk to Vizier directly.
		c.vzToken = conn.Token
		if conn.URL == nil {
			return nil, errors.New("missing Vizier URL, likely still initializing")
		}
		err = c.connect(conn.URL.Host)
	} else {
		err = c.connect(cloudAddr)
	}

	if err != nil {
		return nil, err
	}

	c.vz = pl_api_vizierpb.NewVizierServiceClient(c.conn)

	return c, nil
}

// Connect connects to Vizier (blocking)
func (c *Connector) connect(addr string) error {
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

// PassthroughMode returns true if passthrough mode is enabled.
func (c *Connector) PassthroughMode() bool {
	return c.passthroughEnabled
}

// ExecuteScriptStream execute a vizier query as a stream.
func (c *Connector) ExecuteScriptStream(ctx context.Context, q string) (chan *VizierExecData, error) {
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

	if c.passthroughEnabled {
		var err error
		ctx, err = ctxWithCreds(ctx)
		if err != nil {
			log.WithError(err).Fatalln("Failed to get credentials")
		}
	} else {
		ctx = ctxWithTokenCreds(ctx, c.vzToken)
	}

	resp, err := c.vz.ExecuteScript(ctx, reqPB)
	if err != nil {
		return nil, err
	}

	results := make(chan *VizierExecData)
	go func() {
		for {
			select {
			case <-resp.Context().Done():
				return
			case <-ctx.Done():
				return
			default:
				msg, err := resp.Recv()
				results <- &VizierExecData{resp: msg, err: err}
				if err != nil || msg == nil {
					close(results)
					return
				}
			}
		}

	}()
	return results, nil
}
