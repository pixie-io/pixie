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

package pxapi

import (
	"context"

	"px.dev/pixie/src/api/go/pxapi/errdefs"

	"px.dev/pixie/src/api/proto/vizierpb"
)

// VizierClient is the client for a single vizier.
type VizierClient struct {
	cloud    *Client
	vizierID string
	vzClient vizierpb.VizierServiceClient
	encOpts  *vizierpb.ExecuteScriptRequest_EncryptionOptions
	decOpts  *vizierpb.ExecuteScriptRequest_EncryptionOptions
}

// ExecuteScript runs the script on vizier.
func (v *VizierClient) ExecuteScript(ctx context.Context, pxl string, mux TableMuxer) (*ScriptResults, error) {
	req := &vizierpb.ExecuteScriptRequest{
		ClusterID:         v.vizierID,
		QueryStr:          pxl,
		EncryptionOptions: v.encOpts,
	}
	origCtx := ctx
	ctx, cancel := context.WithCancel(ctx)
	res, err := v.vzClient.ExecuteScript(v.cloud.cloudCtxWithMD(ctx), req)
	if err != nil {
		cancel()
		return nil, err
	}

	sr := newScriptResults()
	sr.c = res
	sr.cancel = cancel
	sr.tm = mux
	sr.decOpts = v.decOpts
	sr.v = v
	sr.origCtx = origCtx

	return sr, nil
}

// GenerateOTelScript generates an otel export script for a given pxl script that has px.display calls.
func (v *VizierClient) GenerateOTelScript(ctx context.Context, pxl string) (string, error) {
	req := &vizierpb.GenerateOTelScriptRequest{
		ClusterID: v.vizierID,
		PxlScript: pxl,
	}
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	res, err := v.vzClient.GenerateOTelScript(v.cloud.cloudCtxWithMD(ctx), req)
	if err != nil {
		return "", err
	}
	if err := errdefs.ParseStatus(res.Status); err != nil {
		return "", err
	}

	return res.OTelScript, nil
}
