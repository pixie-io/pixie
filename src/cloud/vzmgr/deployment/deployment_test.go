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

package deployment_test

import (
	"context"
	"errors"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/vzmgr/deployment"
	"px.dev/pixie/src/cloud/vzmgr/vzerrors"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/utils"
)

var (
	testOrgID  = uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000")
	testUserID = uuid.FromStringOrNil("423e4567-e89b-12d3-a456-426655440000")
	testKeyID  = uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440000")

	testValidClusterID = uuid.FromStringOrNil("553e4567-e89b-12d3-a456-426655440000")

	testValidDeploymentKey = "883e4567-e89b-12d3-a456-426655440000"
)

type fakeDF struct{}

func (f *fakeDF) FetchOrgUserIDUsingDeploymentKey(ctx context.Context, key string) (uuid.UUID, uuid.UUID, uuid.UUID, error) {
	if key == testValidDeploymentKey {
		return testOrgID, testUserID, testKeyID, nil
	}
	return uuid.Nil, uuid.Nil, uuid.Nil, vzerrors.ErrDeploymentKeyNotFound
}

type fakeProvisioner struct {
}

func (f *fakeProvisioner) ProvisionOrClaimVizier(ctx context.Context, orgID uuid.UUID, userID uuid.UUID, clusterUID string, clusterName string) (uuid.UUID, string, error) {
	if testOrgID == orgID && testUserID == userID && clusterUID == "cluster1" && clusterName == "test" {
		return testValidClusterID, clusterName, nil
	}
	if testOrgID == orgID && testUserID == userID && clusterUID == "cluster2" {
		return uuid.Nil, "", vzerrors.ErrProvisionFailedVizierIsActive
	}
	return uuid.Nil, "", errors.New("bad request")
}

func TestService_RegisterVizierDeployment(t *testing.T) {
	svc := deployment.New(&fakeDF{}, &fakeProvisioner{})

	ctx := context.Background()
	resp, err := svc.RegisterVizierDeployment(ctx, &vzmgrpb.RegisterVizierDeploymentRequest{
		K8sClusterUID:  "cluster1",
		DeploymentKey:  testValidDeploymentKey,
		K8sClusterName: "test",
	})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, testValidClusterID, utils.UUIDFromProtoOrNil(resp.VizierID))
}

func TestService_RegisterVizierDeployment_ClusterAlreadyRunning(t *testing.T) {
	svc := deployment.New(&fakeDF{}, &fakeProvisioner{})

	ctx := context.Background()
	resp, err := svc.RegisterVizierDeployment(ctx, &vzmgrpb.RegisterVizierDeploymentRequest{
		K8sClusterUID: "cluster2",
		DeploymentKey: testValidDeploymentKey,
	})
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, codes.ResourceExhausted, status.Code(err))
}

func TestService_RegisterVizierDeployment_InvalidDeployKey(t *testing.T) {
	svc := deployment.New(&fakeDF{}, &fakeProvisioner{})

	ctx := context.Background()
	resp, err := svc.RegisterVizierDeployment(ctx, &vzmgrpb.RegisterVizierDeploymentRequest{
		K8sClusterUID: "cluster2",
		DeploymentKey: "a bad key",
	})
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, codes.Unauthenticated, status.Code(err))
}
