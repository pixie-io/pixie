package vizier

import (
	"context"
	"errors"
	"fmt"
	"os"
	"os/signal"
	"regexp"
	"strings"
	"time"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/shared/services"
	vispb "pixielabs.ai/pixielabs/src/shared/vispb"
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

func lookupVariable(variable string, computedArgs []script.Arg) (string, error) {
	for _, arg := range computedArgs {
		if arg.Name == variable {
			return arg.Value, nil
		}
	}
	return "", fmt.Errorf("variable '%s' not found", variable)
}

func makeFuncToExecute(f *vispb.Widget_Func, computedArgs []script.Arg, name string) (*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute, error) {
	execFunc := &pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute{}
	execFunc.FuncName = f.Name

	execFunc.ArgValues = make([]*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute_ArgValue, len(f.Args))
	for idx, arg := range f.Args {
		var value string
		var err error
		switch x := arg.Input.(type) {
		case *vispb.Widget_Func_FuncArg_Value:
			value = x.Value
		case *vispb.Widget_Func_FuncArg_Variable:
			// Lookup variable.
			value, err = lookupVariable(x.Variable, computedArgs)
			if err != nil {
				return nil, err
			}
		default:
			return nil, fmt.Errorf("Value not found")
		}

		execFunc.ArgValues[idx] = &pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute_ArgValue{
			Name:  arg.Name,
			Value: value,
		}
	}

	execFunc.OutputTablePrefix = "widget"
	if name != "" {
		execFunc.OutputTablePrefix = name
	}

	return execFunc, nil
}

func getFuncsToExecute(script *script.ExecutableScript) ([]*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute, error) {
	if script.Vis == nil {
		return []*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute{}, nil
	}
	// Accumulate the global function definitions.
	execFuncs := []*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute{}
	if script.Vis.GlobalFuncs != nil {
		for _, f := range script.Vis.GlobalFuncs {
			execFunc, err := makeFuncToExecute(f.Func, script.ComputedArgs(), f.OutputName)
			if err != nil {
				return []*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute{}, err
			}
			execFuncs = append(execFuncs, execFunc)
		}

	}
	// Find function definitions within widgets.
	for _, w := range script.Vis.Widgets {
		var f *vispb.Widget_Func
		switch x := w.FuncOrRef.(type) {
		case *vispb.Widget_Func_:
			f = x.Func
		default:
			// Skip if it's not a function definition.
			continue
		}

		execFunc, err := makeFuncToExecute(f, script.ComputedArgs(), w.Name)
		if err != nil {
			return []*pl_api_vizierpb.ExecuteScriptRequest_FuncToExecute{}, err
		}
		execFuncs = append(execFuncs, execFunc)
	}
	return execFuncs, nil
}

func containsMutation(script *script.ExecutableScript) bool {
	r := regexp.MustCompile(`(?m:^(from pxtrace|import pxtrace)$)`)
	return len(r.FindAllStringSubmatch(script.ScriptString, -1)) > 0
}

// ExecuteScriptStream execute a vizier query as a stream.
func (c *Connector) ExecuteScriptStream(ctx context.Context, script *script.ExecutableScript) (chan *VizierExecData, error) {
	scriptStr := strings.TrimSpace(script.ScriptString)
	if len(scriptStr) == 0 {
		return nil, errors.New("input query is empty")
	}

	execFuncs, err := getFuncsToExecute(script)
	if err != nil {
		return nil, err
	}

	reqPB := &pl_api_vizierpb.ExecuteScriptRequest{
		QueryStr:  scriptStr,
		ClusterID: c.id.String(),
		ExecFuncs: execFuncs,
		Mutation:  containsMutation(script),
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
				results <- &VizierExecData{ClusterID: c.id, Resp: msg, Err: err}
				if err != nil || msg == nil {
					close(results)
					return
				}
			}
		}

	}()
	return results, nil
}
