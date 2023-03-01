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

package gke

import (
	"context"

	containerpb "google.golang.org/genproto/googleapis/container/v1"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

func (p *ClusterProvider) getClusterContext(ctx context.Context, c *containerpb.Cluster) (*cluster.Context, error) {
	return nil, nil
}

func (p *ClusterProvider) waitForKubeInternals(ctx context.Context, c *containerpb.Cluster, clusterCtx *cluster.Context) error {
	return nil
}

func (p *ClusterProvider) deleteClusterCleanupFunc(c *containerpb.Cluster) func() {
	return func() {
	}
}

func (p *ClusterProvider) createCluster(ctx context.Context, spec *experimentpb.ClusterSpec) (*containerpb.Cluster, error) {
	return nil, nil
}
