/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package ptproxy

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"

	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/cloud/scriptmgr/scriptmgrpb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/jwtpb"
	jwtutils "px.dev/pixie/src/shared/services/utils"
)

type vzmgrClient interface {
	GetVizierInfo(ctx context.Context, in *uuidpb.UUID, opts ...grpc.CallOption) (*cvmsgspb.VizierInfo, error)
	GetVizierConnectionInfo(ctx context.Context, in *uuidpb.UUID, opts ...grpc.CallOption) (*cvmsgspb.VizierConnectionInfo, error)
}

type scriptmgrClient interface {
	CheckScriptExists(ctx context.Context, req *scriptmgrpb.CheckScriptExistsReq, opts ...grpc.CallOption) (*scriptmgrpb.CheckScriptExistsResp, error)
}

// VizierPassThroughProxy implements the VizierAPI and allows proxying the data to the actual
// vizier cluster.
type VizierPassThroughProxy struct {
	nc                         *nats.Conn
	vc                         vzmgrClient
	sm                         scriptmgrClient
	scriptModificationDisabled bool
}

// getServiceCredentials returns JWT credentials for inter-service requests.
func getServiceCredentials(signingKey string) (string, error) {
	claims := jwtutils.GenerateJWTForService("cloud api", viper.GetString("domain_name"))
	return jwtutils.SignJWTClaims(claims, signingKey)
}

// NewVizierPassThroughProxy creates a new passthrough proxy.
func NewVizierPassThroughProxy(nc *nats.Conn, vc vzmgrClient, sm scriptmgrClient, scriptModificationDisabled bool) *VizierPassThroughProxy {
	return &VizierPassThroughProxy{nc: nc, vc: vc, sm: sm, scriptModificationDisabled: scriptModificationDisabled}
}

func (v *VizierPassThroughProxy) isScriptModified(ctx context.Context, script string) (bool, error) {
	hash := sha256.New()
	hash.Write([]byte(script))
	hashStr := hex.EncodeToString(hash.Sum(nil))
	req := &scriptmgrpb.CheckScriptExistsReq{Sha256Hash: hashStr}

	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	if err != nil {
		return false, err
	}

	resp, err := v.sm.CheckScriptExists(ctx, req)
	if err != nil {
		return false, err
	}
	return !resp.Exists, nil
}

// ExecuteScript is the GRPC stream method.
func (v *VizierPassThroughProxy) ExecuteScript(req *vizierpb.ExecuteScriptRequest, srv vizierpb.VizierService_ExecuteScriptServer) error {
	rp, err := newRequestProxyer(v.vc, v.nc, false, req, srv)
	if err != nil {
		return err
	}
	defer rp.Finish()
	if v.scriptModificationDisabled {
		modified, err := v.isScriptModified(srv.Context(), req.QueryStr)
		if err != nil {
			return err
		}

		if modified {
			return status.Error(codes.InvalidArgument, "Script modification has been disabled")
		}
	}

	vizReq := rp.prepareVizierRequest()
	vizReq.Msg = &cvmsgspb.C2VAPIStreamRequest_ExecReq{ExecReq: req}
	if err := rp.sendMessageToVizier(vizReq); err != nil {
		return err
	}

	return rp.Run()
}

// HealthCheck is the GRPC stream method.
func (v *VizierPassThroughProxy) HealthCheck(req *vizierpb.HealthCheckRequest, srv vizierpb.VizierService_HealthCheckServer) error {
	rp, err := newRequestProxyer(v.vc, v.nc, false, req, srv)
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
func (v *VizierPassThroughProxy) DebugLog(req *vizierpb.DebugLogRequest, srv vizierpb.VizierDebugService_DebugLogServer) error {
	rp, err := newRequestProxyer(v.vc, v.nc, true, req, srv)
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

// generateOTelScriptStream is a stream fake that fits into the request proxyer interface.
type generateOTelScriptStream struct {
	resp *vizierpb.GenerateOTelScriptResponse
	ctx  context.Context
}

func (gs *generateOTelScriptStream) Context() context.Context {
	return gs.ctx
}

func (gs *generateOTelScriptStream) SendMsg(data interface{}) error {
	gs.resp = data.(*vizierpb.GenerateOTelScriptResponse)
	return nil
}

// GenerateOTelScript is the GRPC method to generate an OTel script from a DataFrame script.
func (v *VizierPassThroughProxy) GenerateOTelScript(ctx context.Context, req *vizierpb.GenerateOTelScriptRequest) (*vizierpb.GenerateOTelScriptResponse, error) {
	srv := &generateOTelScriptStream{ctx: ctx}
	rp, err := newRequestProxyer(v.vc, v.nc, true, req, srv)
	if err != nil {
		return nil, err
	}
	defer rp.Finish()
	vizReq := rp.prepareVizierRequest()
	vizReq.Msg = &cvmsgspb.C2VAPIStreamRequest_GenerateOTelScriptReq{GenerateOTelScriptReq: req}
	if err := rp.sendMessageToVizier(vizReq); err != nil {
		return nil, err
	}
	err = rp.Run()
	if err != nil {
		return nil, err
	}
	return srv.resp, nil
}

// DebugPods is the GRPC method to fetch the list of Vizier pods (and statuses) from a cluster.
func (v *VizierPassThroughProxy) DebugPods(req *vizierpb.DebugPodsRequest, srv vizierpb.VizierDebugService_DebugPodsServer) error {
	rp, err := newRequestProxyer(v.vc, v.nc, true, req, srv)
	if err != nil {
		return err
	}
	defer rp.Finish()
	vizReq := rp.prepareVizierRequest()
	vizReq.Msg = &cvmsgspb.C2VAPIStreamRequest_DebugPodsReq{DebugPodsReq: req}
	if err := rp.sendMessageToVizier(vizReq); err != nil {
		return err
	}
	return rp.Run()
}

func getCredsFromCtx(ctx context.Context) (string, *jwtpb.JWTClaims, error) {
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return "", nil, err
	}

	return aCtx.AuthToken, aCtx.Claims, nil
}
