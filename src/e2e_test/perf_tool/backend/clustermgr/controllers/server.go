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

	"px.dev/pixie/src/e2e_test/perf_tool/backend/clustermgr/clustermgrpb"
)

var _ clustermgrpb.ClusterManagerServiceServer = &Server{}

// ClusterDatastore defines the interfaces with postgres containing the current state of clusters managed by clustermgr.
type ClusterDatastore interface {
}

// Server defines an gRPC server type.
type Server struct {
	d ClusterDatastore
}

// NewServer creates GRPC handlers.
func NewServer(d ClusterDatastore) *Server {
	return &Server{
		d: d,
	}
}

// PrepareCluster implements the gRPC method of the same name.
// This RPC is used to queue preparing of a cluster for an experiment.
func (s *Server) PrepareCluster(ctx context.Context, req *clustermgrpb.PrepareClusterRequest) (*clustermgrpb.PrepareClusterResponse, error) {
	return nil, nil
}

// ReturnCluster implements the gRPC method of the same name.
// This RPC is used to notify the clustermgr that the given cluster is no longer needed.
func (s *Server) ReturnCluster(ctx context.Context, req *clustermgrpb.ReturnClusterRequest) (*clustermgrpb.ReturnClusterResponse, error) {
	return nil, nil
}

// GetClusterInfo implements the gRPC method of the same name.
// This RPC is used by the runner service to get the cluster k8s credentials and k8s cluster address for a given cluster.
func (s *Server) GetClusterInfo(ctx context.Context, req *clustermgrpb.GetClusterInfoRequest) (*clustermgrpb.ClusterInfo, error) {
	return nil, nil
}
