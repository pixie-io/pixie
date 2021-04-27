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

package controller

import (
	"context"
	"sort"

	"github.com/gofrs/uuid"
	"github.com/graph-gophers/graphql-go"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/utils"
)

// APIKeyResolver is the resolver responsible for API keys.
type APIKeyResolver struct {
	id          uuid.UUID
	key         string
	createdAtNs int64
	desc        string
}

// ID returns API key ID.
func (d *APIKeyResolver) ID() graphql.ID {
	return graphql.ID(d.id.String())
}

// Key returns the API key value.
func (d *APIKeyResolver) Key() string {
	return d.key
}

// CreatedAtMs returns the time at which the API key was created.
func (d *APIKeyResolver) CreatedAtMs() float64 {
	return float64(d.createdAtNs) / 1e6
}

// Desc returns the description of the key.
func (d *APIKeyResolver) Desc() string {
	return d.desc
}

// CreateAPIKey creates a new API key.
func (q *QueryResolver) CreateAPIKey(ctx context.Context) (*APIKeyResolver, error) {
	grpcAPI := q.Env.APIKeyMgr
	res, err := grpcAPI.Create(ctx, &cloudpb.CreateAPIKeyRequest{})
	if err != nil {
		return nil, err
	}
	return apiKeyToResolver(res)
}

func apiKeyToResolver(key *cloudpb.APIKey) (*APIKeyResolver, error) {
	keyID, err := utils.UUIDFromProto(key.ID)
	if err != nil {
		return nil, err
	}

	return &APIKeyResolver{
		id:          keyID,
		key:         key.Key,
		createdAtNs: key.CreatedAt.Seconds*NanosPerSecond + int64(key.CreatedAt.Nanos),
		desc:        key.Desc,
	}, nil
}

// APIKeys lists all of the API keys.
func (q *QueryResolver) APIKeys(ctx context.Context) ([]*APIKeyResolver, error) {
	grpcAPI := q.Env.APIKeyMgr
	res, err := grpcAPI.List(ctx, &cloudpb.ListAPIKeyRequest{})
	if err != nil {
		return nil, err
	}
	var keys []*APIKeyResolver
	for _, key := range res.Keys {
		resolvedKey, err := apiKeyToResolver(key)
		if err != nil {
			return nil, err
		}
		keys = append(keys, resolvedKey)
	}
	// Sort by descending time
	sort.Slice(keys, func(i, j int) bool { return keys[i].createdAtNs > keys[j].createdAtNs })
	return keys, nil
}

type getOrDeleteAPIKeyArgs struct {
	ID graphql.ID
}

// APIKey gets a specific API key.
func (q *QueryResolver) APIKey(ctx context.Context, args *getOrDeleteAPIKeyArgs) (*APIKeyResolver, error) {
	grpcAPI := q.Env.APIKeyMgr
	res, err := grpcAPI.Get(ctx, &cloudpb.GetAPIKeyRequest{
		ID: utils.ProtoFromUUIDStrOrNil(string(args.ID)),
	})
	if err != nil {
		return nil, err
	}
	return apiKeyToResolver(res.Key)
}

// DeleteAPIKey deletes a specific API key.
func (q *QueryResolver) DeleteAPIKey(ctx context.Context, args *getOrDeleteAPIKeyArgs) (bool, error) {
	grpcAPI := q.Env.APIKeyMgr
	_, err := grpcAPI.Delete(ctx, utils.ProtoFromUUIDStrOrNil(string(args.ID)))
	if err != nil {
		return false, err
	}
	return true, nil
}
