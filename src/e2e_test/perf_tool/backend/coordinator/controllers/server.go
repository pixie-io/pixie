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

	"px.dev/pixie/src/e2e_test/perf_tool/backend/coordinator/coordinatorpb"
)

var _ coordinatorpb.CoordinatorServiceServer = &Server{}
var _ coordinatorpb.BuildNotificationServiceServer = &Server{}
var _ coordinatorpb.ClusterNotificationServiceServer = &Server{}
var _ coordinatorpb.RunnerNotificationServiceServer = &Server{}

// QueueDatastore is the interface for accessing the experiment queue, stored in Postgres.
// The queue is stored in postgres so that its persistent.
type QueueDatastore interface {
}

// Server defines an gRPC server type.
type Server struct {
	datastore QueueDatastore
}

// NewServer creates GRPC handlers.
func NewServer(datastore QueueDatastore) *Server {
	return &Server{
		datastore: datastore,
	}
}

// QueueExperiment implements the gRPC method by the same name.
// This RPC launches the tasks necessary to start the experiment and push the experiment to the datastore.
func (s *Server) QueueExperiment(ctx context.Context, req *coordinatorpb.QueueExperimentRequest) (*coordinatorpb.QueueExperimentResponse, error) {
	return nil, nil
}

// NotifyBuildComplete implements the gRPC method by the same name.
// This RPC is called by the builder service to notify the coordinator that the build for a particular experiment is finished.
func (s *Server) NotifyBuildComplete(ctx context.Context, req *coordinatorpb.TaskCompleteRequest) (*coordinatorpb.TaskCompleteResponse, error) {
	return nil, nil
}

// NotifyPrepareClusterComplete implements the gRPC method by the same name.
// This RPC is called by the clustermgr service to notify the coordinator that a cluster has been prepared an assigned to a particular experiment.
func (s *Server) NotifyPrepareClusterComplete(ctx context.Context, req *coordinatorpb.TaskCompleteRequest) (*coordinatorpb.TaskCompleteResponse, error) {
	return nil, nil
}

// NotifyRunComplete implements the gRPC method by the same name.
// This RPC is called by the runner service after an experiment is finished running.
func (s *Server) NotifyRunComplete(ctx context.Context, req *coordinatorpb.TaskCompleteRequest) (*coordinatorpb.TaskCompleteResponse, error) {
	return nil, nil
}
