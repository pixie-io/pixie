package ptproxy

import (
	"context"

	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc"

	proto1 "pixielabs.ai/pixielabs/src/common/uuid/proto"

	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	jwt "pixielabs.ai/pixielabs/src/shared/services/proto"
	pl_api_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

type vizierConnInfo struct {
	vch chan *cvmsgspb.V2CMessage
}

type vzmgrClient interface {
	GetVizierInfo(ctx context.Context, in *proto1.UUID, opts ...grpc.CallOption) (*cvmsgspb.VizierInfo, error)
	GetVizierConnectionInfo(ctx context.Context, in *proto1.UUID, opts ...grpc.CallOption) (*cvmsgspb.VizierConnectionInfo, error)
}

// VizierPassThroughProxy implements the VizierAPI and allows proxying the data to the actual
// vizier cluster.
type VizierPassThroughProxy struct {
	nc *nats.Conn
	vc vzmgrClient

	// Map from request ID to a channel for each goroutine.
	vizierConns map[uuid.UUID]*vizierConnInfo
}

// NewVizierPassThroughProxy creates a new passthrough proxy.
func NewVizierPassThroughProxy(nc *nats.Conn, vc vzmgrClient) *VizierPassThroughProxy {
	return &VizierPassThroughProxy{nc: nc, vc: vc}
}

// ExecuteScript is the GRPC stream method.
func (v *VizierPassThroughProxy) ExecuteScript(req *pl_api_vizierpb.ExecuteScriptRequest, srv pl_api_vizierpb.VizierService_ExecuteScriptServer) error {
	rp, err := newRequestProxyer(v.vc, v.nc, req, srv)
	if err != nil {
		return err
	}
	defer rp.Finish()
	vizReq := rp.prepareVizierRequest()
	vizReq.Msg = &cvmsgspb.C2VAPIStreamRequest_ExecReq{ExecReq: req}
	if err := rp.sendMessageToVizier(vizReq); err != nil {
		return err
	}

	return rp.Run()
}

// HealthCheck is the GRPC stream method.
func (v *VizierPassThroughProxy) HealthCheck(req *pl_api_vizierpb.HealthCheckRequest, srv pl_api_vizierpb.VizierService_HealthCheckServer) error {
	rp, err := newRequestProxyer(v.vc, v.nc, req, srv)
	if err != nil {
		return err
	}
	defer rp.Finish()

	vizReq := rp.prepareVizierRequest()
	vizReq.Msg = &cvmsgspb.C2VAPIStreamRequest_HcReq{HcReq: req}
	if err := rp.sendMessageToVizier(vizReq); err != nil {
		return err
	}

	return rp.Run()
}

// DebugLog is the GRPC stream method to fetch debug logs from vizier.
func (v *VizierPassThroughProxy) DebugLog(req *pl_api_vizierpb.DebugLogRequest, srv pl_api_vizierpb.VizierDebugService_DebugLogServer) error {
	rp, err := newRequestProxyer(v.vc, v.nc, req, srv)
	if err != nil {
		return err
	}
	defer rp.Finish()
	vizReq := rp.prepareVizierRequest()
	vizReq.Msg = &cvmsgspb.C2VAPIStreamRequest_DebugLogReq{DebugLogReq: req}
	if err := rp.sendMessageToVizier(vizReq); err != nil {
		return err
	}

	return rp.Run()
}

func getCredsFromCtx(ctx context.Context) (token string, claim *jwt.JWTClaims, err error) {
	var aCtx *authcontext.AuthContext
	aCtx, err = authcontext.FromContext(ctx)
	if err != nil {
		return
	}

	token = aCtx.AuthToken
	claim = aCtx.Claims
	return
}
