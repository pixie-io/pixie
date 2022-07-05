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

package vizier

import (
	"context"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/utils"
)

// Lister allows fetching information about Viziers from the cloud.
type Lister struct {
	vc cloudpb.VizierClusterInfoClient
}

// NewLister returns a Lister.
func NewLister(cloudAddr string) (*Lister, error) {
	vc, err := newVizierClusterInfoClient(cloudAddr)
	if err != nil {
		return nil, err
	}
	return &Lister{vc: vc}, nil
}

// GetViziersInfo returns information about connected viziers.
func (l *Lister) GetViziersInfo() ([]*cloudpb.ClusterInfo, error) {
	ctx := auth.CtxWithCreds(context.Background())

	c, err := l.vc.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}
	return c.Clusters, nil
}

// GetVizierInfo returns information about a connected vizier.
func (l *Lister) GetVizierInfo(id uuid.UUID) ([]*cloudpb.ClusterInfo, error) {
	ctx := auth.CtxWithCreds(context.Background())
	clusterIDPb := utils.ProtoFromUUID(id)

	c, err := l.vc.GetClusterInfo(ctx, &cloudpb.GetClusterInfoRequest{ID: clusterIDPb})
	if err != nil {
		return nil, err
	}
	return c.Clusters, nil
}
