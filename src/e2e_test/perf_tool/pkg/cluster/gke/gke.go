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

	container "cloud.google.com/go/container/apiv1"
	log "github.com/sirupsen/logrus"
	"google.golang.org/api/option"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

// ClusterOptions stores options to use when creating GKE clusters, eg. which gcloud project to use.
type ClusterOptions struct {
	Project       string
	Zone          string
	Network       string
	Subnet        string
	DynamicSubnet bool
	CIDR          string
	ServicesCIDR  string
	SecurityGroup string
	DiskType      string
	DiskSizeGB    int32
}

const (
	defaultCIDR         = "/21"
	defaultServicesCIDR = "/20"
	defaultDiskType     = "pd-ssd"
	defaultDiskSizeGB   = 100
)

// ClusterProvider creates a new GKE cluster for each call to GetCluster.
type ClusterProvider struct {
	client      *container.ClusterManagerClient
	clusterOpts *ClusterOptions
}

var _ cluster.Provider = &ClusterProvider{}

// NewClusterProvider returns a new ClusterProvider that can create new GKE clusters.
func NewClusterProvider(clusterOpts *ClusterOptions, opts ...option.ClientOption) (*ClusterProvider, error) {
	client, err := container.NewClusterManagerClient(context.Background(), opts...)
	if err != nil {
		return nil, err
	}
	addDefaults(clusterOpts)
	return &ClusterProvider{
		client:      client,
		clusterOpts: clusterOpts,
	}, nil
}

func addDefaults(clusterOpts *ClusterOptions) {
	if clusterOpts.CIDR == "" {
		clusterOpts.CIDR = defaultCIDR
	}
	if clusterOpts.ServicesCIDR == "" {
		clusterOpts.ServicesCIDR = defaultServicesCIDR
	}
	if clusterOpts.DiskType == "" {
		clusterOpts.DiskType = defaultDiskType
	}
	if clusterOpts.DiskSizeGB == 0 {
		clusterOpts.DiskSizeGB = defaultDiskSizeGB
	}
	if clusterOpts.Subnet == "" {
		clusterOpts.DynamicSubnet = true
	}
}

// GetCluster returns a new GKE cluster that adheres to the given spec.
func (p *ClusterProvider) GetCluster(ctx context.Context, spec *experimentpb.ClusterSpec) (*cluster.Context, func(), error) {
	c, err := p.createCluster(ctx, spec)
	if err != nil {
		return nil, nil, err
	}
	log.WithField("cluster_name", c.Name).Trace("Successfully created cluster")
	clusterCtx, err := p.getClusterContext(ctx, c)
	if err != nil {
		return nil, nil, err
	}
	cleanup := p.deleteClusterCleanupFunc(c)

	if err := p.waitForKubeInternals(ctx, c, clusterCtx); err != nil {
		cleanup()
		return nil, nil, err
	}
	return clusterCtx, cleanup, nil
}

// Close closes the GKE client.
func (p *ClusterProvider) Close() error {
	return p.client.Close()
}
