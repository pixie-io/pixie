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

package controllers

import (
	"context"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/scriptmgr/scriptmgrpb"
	"px.dev/pixie/src/utils"
)

// ScriptMgrServer is the server that implements the ScriptMgr gRPC service.
type ScriptMgrServer struct {
	ScriptMgr scriptmgrpb.ScriptMgrServiceClient
}

// GetLiveViews returns a list of all available live views.
func (s *ScriptMgrServer) GetLiveViews(ctx context.Context, req *cloudpb.GetLiveViewsReq) (*cloudpb.GetLiveViewsResp, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	smReq := &scriptmgrpb.GetLiveViewsReq{}
	smResp, err := s.ScriptMgr.GetLiveViews(ctx, smReq)
	if err != nil {
		return nil, err
	}
	resp := &cloudpb.GetLiveViewsResp{
		LiveViews: make([]*cloudpb.LiveViewMetadata, len(smResp.LiveViews)),
	}
	for i, liveView := range smResp.LiveViews {
		resp.LiveViews[i] = &cloudpb.LiveViewMetadata{
			ID:   utils.UUIDFromProtoOrNil(liveView.ID).String(),
			Name: liveView.Name,
			Desc: liveView.Desc,
		}
	}
	return resp, nil
}

// GetLiveViewContents returns the pxl script, vis info, and metdata for a live view.
func (s *ScriptMgrServer) GetLiveViewContents(ctx context.Context, req *cloudpb.GetLiveViewContentsReq) (*cloudpb.GetLiveViewContentsResp, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	smReq := &scriptmgrpb.GetLiveViewContentsReq{
		LiveViewID: utils.ProtoFromUUIDStrOrNil(req.LiveViewID),
	}
	smResp, err := s.ScriptMgr.GetLiveViewContents(ctx, smReq)
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetLiveViewContentsResp{
		Metadata: &cloudpb.LiveViewMetadata{
			ID:   req.LiveViewID,
			Name: smResp.Metadata.Name,
			Desc: smResp.Metadata.Desc,
		},
		PxlContents: smResp.PxlContents,
		Vis:         smResp.Vis,
	}, nil
}

// GetScripts returns a list of all available scripts.
func (s *ScriptMgrServer) GetScripts(ctx context.Context, req *cloudpb.GetScriptsReq) (*cloudpb.GetScriptsResp, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	smReq := &scriptmgrpb.GetScriptsReq{}
	smResp, err := s.ScriptMgr.GetScripts(ctx, smReq)
	if err != nil {
		return nil, err
	}
	resp := &cloudpb.GetScriptsResp{
		Scripts: make([]*cloudpb.ScriptMetadata, len(smResp.Scripts)),
	}
	for i, script := range smResp.Scripts {
		resp.Scripts[i] = &cloudpb.ScriptMetadata{
			ID:          utils.UUIDFromProtoOrNil(script.ID).String(),
			Name:        script.Name,
			Desc:        script.Desc,
			HasLiveView: script.HasLiveView,
		}
	}
	return resp, nil
}

// GetScriptContents returns the pxl string of the script.
func (s *ScriptMgrServer) GetScriptContents(ctx context.Context, req *cloudpb.GetScriptContentsReq) (*cloudpb.GetScriptContentsResp, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	smReq := &scriptmgrpb.GetScriptContentsReq{
		ScriptID: utils.ProtoFromUUIDStrOrNil(req.ScriptID),
	}
	smResp, err := s.ScriptMgr.GetScriptContents(ctx, smReq)
	if err != nil {
		return nil, err
	}
	return &cloudpb.GetScriptContentsResp{
		Metadata: &cloudpb.ScriptMetadata{
			ID:          req.ScriptID,
			Name:        smResp.Metadata.Name,
			Desc:        smResp.Metadata.Desc,
			HasLiveView: smResp.Metadata.HasLiveView,
		},
		Contents: smResp.Contents,
	}, nil
}
