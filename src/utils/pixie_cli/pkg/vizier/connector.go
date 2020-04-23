package vizier

import (
	"context"
	"errors"
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
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
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

// PassthroughMode returns true if passthrough mode is enabled.
func (c *Connector) PassthroughMode() bool {
	return c.passthroughEnabled
}

// ExecuteScriptStream execute a vizier query as a stream.
func (c *Connector) ExecuteScriptStream(ctx context.Context, script *script.ExecutableScript) (chan *VizierExecData, error) {
	scriptStr := strings.TrimSpace(script.ScriptString)
	if len(scriptStr) == 0 {
		return nil, errors.New("input query is empty")
	}

	var execFuncs []*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute
	if script.Vis != nil && script.Vis.Widgets != nil {
		execFuncs = make([]*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute, len(script.Vis.Widgets))
		for idx, w := range script.Vis.Widgets {
			execFunc := &pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute{}
			execFunc.FuncName = w.Func.Name
			execFunc.OutputTablePrefix = "widget"
			if w.Name != "" {
				execFunc.OutputTablePrefix = w.Name
			}
			args := script.ComputedArgs()
			execFunc.ArgValues = make([]*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute_ArgValue, len(args))
			// TODO(zasgar): Add argument support.
			// TODO(zasgar): Deduplicate, exact funcs since table output does not make sense for it.
			for idx, arg := range args {
				execFunc.ArgValues[idx] = &pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute_ArgValue{
					Name:  arg.Name,
					Value: arg.Value,
				}
			}
			execFuncs[idx] = execFunc
		}
	}

	reqPB := &pl_api_vizierpb.ExecuteScriptRequest{
		QueryStr:  scriptStr,
		ClusterID: c.id.String(),
		ExecFuncs: execFuncs,
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
				results <- &VizierExecData{Resp: msg, Err: err}
				if err != nil || msg == nil {
					close(results)
					return
				}
			}
		}

	}()
	return results, nil
}
