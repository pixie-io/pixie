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

	"px.dev/pixie/src/e2e_test/perf_tool/backend/builder/builderpb"
)

var _ builderpb.BuilderServiceServer = &Server{}
var _ builderpb.BuilderWorkerCoordinationServiceServer = &Server{}

// BuilderDatastore is the interface for accessing the database which stores build artifacts and the build queue.
type BuilderDatastore interface {
}

// Server defines an gRPC server type.
type Server struct {
	d BuilderDatastore
}

// NewServer creates GRPC handlers.
func NewServer(d BuilderDatastore) *Server {
	return &Server{
		d: d,
	}
}

// StartBuild queues a build. When there are available resources, the build will be started.
func (s *Server) StartBuild(ctx context.Context, req *builderpb.StartBuildRequest) (*builderpb.StartBuildResponse, error) {
	return nil, nil
}

// GetBuildArtifacts returns the build artifacts for the given experiment ID.
func (s *Server) GetBuildArtifacts(ctx context.Context, req *builderpb.GetBuildArtifactsRequest) (*builderpb.GetBuildArtifactsResponse, error) {
	return nil, nil
}

// GetBuildJob gets the build spec for a given experiment ID. This is used by the k8s build job to access the spec.
func (s *Server) GetBuildJob(ctx context.Context, req *builderpb.GetBuildJobRequest) (*builderpb.GetBuildJobResponse, error) {
	return nil, nil
}

// InsertBuildArtifacts adds the given build artifacts to the datastore. This is called by the k8s build job at the end of its execution.
func (s *Server) InsertBuildArtifacts(ctx context.Context, req *builderpb.InsertBuildArtifactsRequest) (*builderpb.InsertBuildArtifactsResponse, error) {
	return nil, nil
}

// BuildFailed notifies the builder service that a k8s build job has failed.
func (s *Server) BuildFailed(ctx context.Context, req *builderpb.BuildFailedRequest) (*builderpb.BuildFailedResponse, error) {
	return nil, nil
}
