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

	"github.com/gogo/protobuf/jsonpb"
	"github.com/graph-gophers/graphql-go"

	"px.dev/pixie/src/api/proto/cloudpb"
)

// LiveViewMetadataResolver resolves metadata about a live view.
type LiveViewMetadataResolver struct {
	ID   graphql.ID
	Name string
	Desc string
}

// LiveViews lists available live views.
func (q *QueryResolver) LiveViews(ctx context.Context) ([]LiveViewMetadataResolver, error) {
	grpcAPI := q.Env.ScriptMgrServer

	req := &cloudpb.GetLiveViewsReq{}

	resp, err := grpcAPI.GetLiveViews(ctx, req)
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	resolver := make([]LiveViewMetadataResolver, len(resp.LiveViews))
	for i, liveView := range resp.LiveViews {
		resolver[i] = LiveViewMetadataResolver{
			ID:   graphql.ID(liveView.ID),
			Name: liveView.Name,
			Desc: liveView.Desc,
		}
	}
	return resolver, nil
}

// LiveViewContentsResolver resolves the content of a given live view.
type LiveViewContentsResolver struct {
	Metadata    LiveViewMetadataResolver
	PxlContents string
	VisJSON     string
}

type liveViewContentsArgs struct {
	ID graphql.ID
}

// LiveViewContents returns the contents for a given live view.
func (q *QueryResolver) LiveViewContents(ctx context.Context, args *liveViewContentsArgs) (*LiveViewContentsResolver, error) {
	grpcAPI := q.Env.ScriptMgrServer

	req := &cloudpb.GetLiveViewContentsReq{
		LiveViewID: string(args.ID),
	}
	resp, err := grpcAPI.GetLiveViewContents(ctx, req)
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	visJSON := ""
	if resp.Vis != nil {
		m := jsonpb.Marshaler{}
		visJSON, err = m.MarshalToString(resp.Vis)
		if err != nil {
			return nil, err
		}
	}

	return &LiveViewContentsResolver{
		Metadata: LiveViewMetadataResolver{
			ID:   graphql.ID(resp.Metadata.ID),
			Name: resp.Metadata.Name,
			Desc: resp.Metadata.Desc,
		},
		PxlContents: resp.PxlContents,
		VisJSON:     visJSON,
	}, nil
}

// ScriptMetadataResolver resolves metadata about a script.
type ScriptMetadataResolver struct {
	ID          graphql.ID
	Name        string
	Desc        string
	HasLiveView bool
}

// Scripts lists available scripts.
func (q *QueryResolver) Scripts(ctx context.Context) ([]ScriptMetadataResolver, error) {
	grpcAPI := q.Env.ScriptMgrServer

	req := &cloudpb.GetScriptsReq{}

	resp, err := grpcAPI.GetScripts(ctx, req)
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	resolver := make([]ScriptMetadataResolver, len(resp.Scripts))
	for i, script := range resp.Scripts {
		resolver[i] = ScriptMetadataResolver{
			ID:          graphql.ID(script.ID),
			Name:        script.Name,
			Desc:        script.Desc,
			HasLiveView: script.HasLiveView,
		}
	}
	return resolver, nil
}

// ScriptContentsResolver resolves the content of a given live view.
type ScriptContentsResolver struct {
	Metadata ScriptMetadataResolver
	Contents string
}

type scriptContentsArgs struct {
	ID graphql.ID
}

// ScriptContents returns the contents for a given live view.
func (q *QueryResolver) ScriptContents(ctx context.Context, args *scriptContentsArgs) (*ScriptContentsResolver, error) {
	grpcAPI := q.Env.ScriptMgrServer

	req := &cloudpb.GetScriptContentsReq{
		ScriptID: string(args.ID),
	}
	resp, err := grpcAPI.GetScriptContents(ctx, req)
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	return &ScriptContentsResolver{
		Metadata: ScriptMetadataResolver{
			ID:          graphql.ID(resp.Metadata.ID),
			Name:        resp.Metadata.Name,
			Desc:        resp.Metadata.Desc,
			HasLiveView: resp.Metadata.HasLiveView,
		},
		Contents: resp.Contents,
	}, nil
}
