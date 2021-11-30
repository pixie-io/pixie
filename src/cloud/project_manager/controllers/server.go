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
	"fmt"
	"regexp"
	"strings"

	"github.com/gofrs/uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/project_manager/datastore"
	"px.dev/pixie/src/cloud/project_manager/projectmanagerpb"
	"px.dev/pixie/src/utils"
)

var projectRegex = regexp.MustCompile("^([a-z0-9])+(-[a-z0-9]+)*$")

// ProjectDatastore is the required interface for the backing data model.
type ProjectDatastore interface {
	CheckAvailability(uuid.UUID, string) (bool, error)
	RegisterProject(uuid.UUID, string) error
	GetProjectForOrg(uuid.UUID) (*datastore.ProjectInfo, error)
	GetProjectByName(uuid.UUID, string) (*datastore.ProjectInfo, error)
}

// Server defines an gRPC server type.
type Server struct {
	datastore ProjectDatastore
}

// NewServer creates GRPC handlers.
func NewServer(datastore ProjectDatastore) *Server {
	return &Server{
		datastore: datastore,
	}
}

var projectNameBlockList = map[string]bool{}

func validProject(s string) bool {
	return projectRegex.MatchString(s)
}

// IsProjectAvailable checks to see if a project is available.
func (s *Server) IsProjectAvailable(ctx context.Context, req *projectmanagerpb.IsProjectAvailableRequest) (*projectmanagerpb.IsProjectAvailableResponse, error) {
	resp := &projectmanagerpb.IsProjectAvailableResponse{}
	pn := strings.ToLower(req.ProjectName)
	if _, exists := projectNameBlockList[pn]; exists {
		resp.Available = false
		return resp, nil
	}

	if !validProject(pn) {
		return nil, status.Error(codes.InvalidArgument, "project name must consist of only a-z and 0-9")
	}

	orgID, err := utils.UUIDFromProto(req.OrgID)
	if err != nil {
		return nil, err
	}

	isAvailable, err := s.datastore.CheckAvailability(orgID, pn)
	if err != nil {
		return nil, err
	}

	resp.Available = isAvailable
	return resp, nil
}

// RegisterProject registers a new project..
func (s *Server) RegisterProject(ctx context.Context, req *projectmanagerpb.RegisterProjectRequest) (*projectmanagerpb.RegisterProjectResponse, error) {
	resp := &projectmanagerpb.RegisterProjectResponse{}
	pn := strings.ToLower(req.ProjectName)

	if !validProject(pn) {
		return nil, status.Error(codes.InvalidArgument, "project name must consist of only a-z and 0-9")
	}

	parsedOrgID, err := utils.UUIDFromProto(req.OrgID)
	if err != nil {
		return nil, err
	}

	isAvailable, err := s.datastore.CheckAvailability(parsedOrgID, pn)
	if err != nil {
		return nil, err
	}
	if !isAvailable {
		return nil, status.Error(codes.InvalidArgument, fmt.Sprintf("project '%s' already exists for org", pn))
	}

	// TODO(zasgar/michellenguyen, PC-829): Provide more context about potential failures by returning an
	// error instead of boolean.
	err = s.datastore.RegisterProject(parsedOrgID, pn)
	if err != nil {
		resp.ProjectRegistered = false
		return resp, err
	}

	resp.ProjectRegistered = true
	return resp, nil
}

// GetProjectForOrg gets the project information based on the passed in ID.
func (s *Server) GetProjectForOrg(ctx context.Context, req *uuidpb.UUID) (*projectmanagerpb.ProjectInfo, error) {
	parsedOrgID, err := utils.UUIDFromProto(req)
	if err != nil {
		return nil, err
	}

	projectInfo, err := s.datastore.GetProjectForOrg(parsedOrgID)

	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	if projectInfo == nil {
		return nil, status.Error(codes.NotFound, "not found")
	}

	resp := &projectmanagerpb.ProjectInfo{}
	resp.ProjectName = projectInfo.ProjectName
	resp.OrgID = utils.ProtoFromUUID(projectInfo.OrgID)

	return resp, nil
}

// GetProjectByName gets the project information based on the passed in project name.
func (s *Server) GetProjectByName(ctx context.Context, req *projectmanagerpb.GetProjectByNameRequest) (*projectmanagerpb.ProjectInfo, error) {
	pn := strings.ToLower(req.ProjectName)

	if len(pn) == 0 {
		return nil, status.Error(codes.InvalidArgument, "project name is a required argument")
	}

	orgID, err := utils.UUIDFromProto(req.OrgID)
	if err != nil {
		return nil, err
	}

	projectInfo, err := s.datastore.GetProjectByName(orgID, pn)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	if projectInfo == nil {
		return nil, status.Error(codes.NotFound, "not found")
	}

	resp := &projectmanagerpb.ProjectInfo{}
	resp.ProjectName = projectInfo.ProjectName

	if orgID != projectInfo.OrgID {
		return nil, status.Error(codes.Internal, "mismatched org id for GetProjectByName")
	}

	resp.OrgID = utils.ProtoFromUUID(projectInfo.OrgID)

	return resp, nil
}
