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
	"sort"

	"github.com/gofrs/uuid"
	"github.com/graph-gophers/graphql-go"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/utils"
)

// NanosPerSecond is the number of nanoseconds per second.
const NanosPerSecond int64 = 1000 * 1000 * 1000

// DeploymentKeyMetadataResolver is the resolver responsible for deploy key metadata.
type DeploymentKeyMetadataResolver struct {
	id          uuid.UUID
	createdAtNs int64
	desc        string
}

// ID returns deployment key ID.
func (d *DeploymentKeyMetadataResolver) ID() graphql.ID {
	return graphql.ID(d.id.String())
}

// CreatedAtMs returns the time at which the deployment key was created.
func (d *DeploymentKeyMetadataResolver) CreatedAtMs() float64 {
	return float64(d.createdAtNs) / 1e6
}

// Desc returns the description of the key.
func (d *DeploymentKeyMetadataResolver) Desc() string {
	return d.desc
}

// DeploymentKeyResolver resolves metadata and the current key value for a single key.
type DeploymentKeyResolver struct {
	DeploymentKeyMetadataResolver
	key string
}

// Key returns the deployment key value.
func (d *DeploymentKeyResolver) Key() string {
	return d.key
}

// CreateDeploymentKey creates a new deployment key.
func (q *QueryResolver) CreateDeploymentKey(ctx context.Context) (*DeploymentKeyResolver, error) {
	grpcAPI := q.Env.VizierDeployKeyMgr
	res, err := grpcAPI.Create(ctx, &cloudpb.CreateDeploymentKeyRequest{})
	if err != nil {
		return nil, rpcErrorHelper(err)
	}
	return deploymentKeyToResolver(res)
}

func deploymentKeyToResolver(key *cloudpb.DeploymentKey) (*DeploymentKeyResolver, error) {
	keyID, err := utils.UUIDFromProto(key.ID)
	if err != nil {
		return nil, err
	}

	return &DeploymentKeyResolver{
		DeploymentKeyMetadataResolver: DeploymentKeyMetadataResolver{
			id:          keyID,
			createdAtNs: key.CreatedAt.Seconds*NanosPerSecond + int64(key.CreatedAt.Nanos),
			desc:        key.Desc,
		},
		key: key.Key,
	}, nil
}

func deploymentKeyMetadatasToResolver(mds []*cloudpb.DeploymentKeyMetadata) ([]*DeploymentKeyMetadataResolver, error) {
	var mdrs []*DeploymentKeyMetadataResolver
	for _, md := range mds {
		mdu, err := utils.UUIDFromProto(md.ID)
		if err != nil {
			return nil, err
		}
		resolved := &DeploymentKeyMetadataResolver{
			id:          mdu,
			createdAtNs: md.CreatedAt.Seconds*NanosPerSecond + int64(md.CreatedAt.Nanos),
			desc:        md.Desc,
		}
		mdrs = append(mdrs, resolved)
	}
	sort.Slice(mdrs, func(i, j int) bool { return mdrs[i].createdAtNs > mdrs[j].createdAtNs })
	return mdrs, nil
}

// DeploymentKeys lists all of the deployment keys.
func (q *QueryResolver) DeploymentKeys(ctx context.Context) ([]*DeploymentKeyMetadataResolver, error) {
	grpcAPI := q.Env.VizierDeployKeyMgr
	res, err := grpcAPI.List(ctx, &cloudpb.ListDeploymentKeyRequest{})
	if err != nil {
		return nil, err
	}
	return deploymentKeyMetadatasToResolver(res.Keys)
}

type getOrDeleteDeployKeyArgs struct {
	ID graphql.ID
}

// DeploymentKey gets a specific deployment key.
func (q *QueryResolver) DeploymentKey(ctx context.Context, args *getOrDeleteDeployKeyArgs) (*DeploymentKeyResolver, error) {
	grpcAPI := q.Env.VizierDeployKeyMgr
	res, err := grpcAPI.Get(ctx, &cloudpb.GetDeploymentKeyRequest{
		ID: utils.ProtoFromUUIDStrOrNil(string(args.ID)),
	})
	if err != nil {
		return nil, rpcErrorHelper(err)
	}
	return deploymentKeyToResolver(res.Key)
}

// DeleteDeploymentKey deletes a specific deployment key.
func (q *QueryResolver) DeleteDeploymentKey(ctx context.Context, args *getOrDeleteDeployKeyArgs) (bool, error) {
	grpcAPI := q.Env.VizierDeployKeyMgr
	_, err := grpcAPI.Delete(ctx, utils.ProtoFromUUIDStrOrNil(string(args.ID)))
	if err != nil {
		return false, rpcErrorHelper(err)
	}
	return true, nil
}
