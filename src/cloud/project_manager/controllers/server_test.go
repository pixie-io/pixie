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

package controllers_test

import (
	"context"
	"errors"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	controllers "px.dev/pixie/src/cloud/project_manager/controllers"
	"px.dev/pixie/src/cloud/project_manager/datastore"
	"px.dev/pixie/src/cloud/project_manager/projectmanagerpb"
	"px.dev/pixie/src/utils"
)

type fakeDatastore struct {
}

func (d *fakeDatastore) RegisterProject(orgID uuid.UUID, projectName string) error {
	return nil
}

func (d *fakeDatastore) CheckAvailability(uuid uuid.UUID, projectName string) (bool, error) {
	if projectName == "is-available" {
		return true, nil
	}

	if projectName == "should-error" {
		return false, errors.New("something bad happened")
	}

	return false, nil
}

func (d *fakeDatastore) GetProjectForOrg(org uuid.UUID) (*datastore.ProjectInfo, error) {
	if org.String() == "123e4567-e89b-12d3-a456-426655440000" {
		return &datastore.ProjectInfo{
			OrgID:       uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			ProjectName: "my-org",
		}, nil
	}
	if org.String() == "323e4567-e89b-12d3-a456-426655440000" {
		return nil, errors.New("internal error")
	}
	return nil, nil
}

func (d *fakeDatastore) GetProjectByName(org uuid.UUID, projectName string) (*datastore.ProjectInfo, error) {
	if projectName == "my-org" {
		return &datastore.ProjectInfo{
			OrgID:       org,
			ProjectName: "my-org",
		}, nil
	}
	if projectName == "goo" {
		return nil, errors.New("badness")
	}
	// Not found.
	return nil, nil
}

func SetupServerTest(t *testing.T) *controllers.Server {
	server := controllers.NewServer(&fakeDatastore{})
	assert.NotNil(t, server)
	return server
}

func TestServer_IsProjectAvailable(t *testing.T) {
	server := SetupServerTest(t)

	tests := []struct {
		name        string
		projectName string
		isAvailable bool
		expectError bool
	}{
		{
			name:        "Valid project should return available",
			projectName: "is-available",
			isAvailable: true,
			expectError: false,
		},
		{
			name:        "Should ignore case on project check",
			projectName: "iS-Available",
			isAvailable: true,
			expectError: false,
		},
		{
			name:        "Error on DB lookup should return error",
			projectName: "should-error",
			isAvailable: false,
			expectError: true,
		},
		{
			name:        "Used project should return not-available",
			projectName: "already-used",
			isAvailable: false,
			expectError: false,
		},
		{
			name:        "Blocked projects are not available",
			projectName: "cloud",
			isAvailable: false,
			expectError: false,
		},
		{
			name:        "invalid project name should be an error",
			projectName: "-abc",
			isAvailable: false,
			expectError: true,
		},
		{
			name:        "invalid project name should be an error",
			projectName: "A--abc",
			isAvailable: false,
			expectError: true,
		},
		{
			name:        "invalid project name should be an error",
			projectName: "a_b",
			isAvailable: false,
			expectError: true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			req := &projectmanagerpb.IsProjectAvailableRequest{
				OrgID:       utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
				ProjectName: test.projectName,
			}
			resp, err := server.IsProjectAvailable(context.Background(), req)
			if test.expectError {
				assert.NotNil(t, err)
			} else {
				require.NoError(t, err)
				assert.Equal(t, resp.Available, test.isAvailable)
			}
		})
	}
}

func TestServer_GetProjectByName(t *testing.T) {
	server := SetupServerTest(t)

	t.Run("project found", func(t *testing.T) {
		req := &projectmanagerpb.GetProjectByNameRequest{
			OrgID:       utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
			ProjectName: "my-org",
		}
		resp, err := server.GetProjectByName(context.Background(), req)
		require.NoError(t, err)
		require.NotNil(t, resp)

		assert.Equal(t, resp.ProjectName, "my-org")
		assert.Equal(t, resp.OrgID, utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"))
	})

	t.Run("project not found", func(t *testing.T) {
		req := &projectmanagerpb.GetProjectByNameRequest{
			OrgID:       utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
			ProjectName: "someproject",
		}
		resp, err := server.GetProjectByName(context.Background(), req)
		assert.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.NotFound, s.Code())
	})

	t.Run("failed datastore request", func(t *testing.T) {
		req := &projectmanagerpb.GetProjectByNameRequest{
			OrgID:       utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
			ProjectName: "goo",
		}
		resp, err := server.GetProjectByName(context.Background(), req)
		require.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.Internal, s.Code())
	})

	t.Run("request with empty project", func(t *testing.T) {
		req := &projectmanagerpb.GetProjectByNameRequest{
			OrgID: utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
		}
		resp, err := server.GetProjectByName(context.Background(), req)
		require.NotNil(t, err)
		assert.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.InvalidArgument, s.Code())
	})
}

func TestServer_GetProjectForOrg(t *testing.T) {
	server := SetupServerTest(t)

	t.Run("project found", func(t *testing.T) {
		resp, err := server.GetProjectForOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"))
		require.NoError(t, err)
		require.NotNil(t, resp)

		assert.Equal(t, resp.ProjectName, "my-org")
		assert.Equal(t, resp.OrgID, utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"))
	})

	t.Run("not found", func(t *testing.T) {
		resp, err := server.GetProjectForOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"))
		require.NotNil(t, err)
		require.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.NotFound, s.Code())
	})

	t.Run("error with backend", func(t *testing.T) {
		resp, err := server.GetProjectForOrg(context.Background(), utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"))
		require.NotNil(t, err)
		require.Nil(t, resp)

		s, ok := status.FromError(err)
		require.True(t, ok)
		assert.Equal(t, codes.Internal, s.Code())
	})
}
