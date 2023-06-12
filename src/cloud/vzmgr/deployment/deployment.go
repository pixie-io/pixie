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

package deployment

import (
	"context"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/vzmgr/vzerrors"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/utils"
)

// InfoFetcher fetches information about deployments using the key.
type InfoFetcher interface {
	FetchOrgUserIDUsingDeploymentKey(context.Context, string) (uuid.UUID, uuid.UUID, uuid.UUID, error)
}

// VizierProvisioner provisions a new Vizier.
type VizierProvisioner interface {
	// ProvisionVizier creates the vizier, with specified org_id, user_id, cluster_uid. Returns
	// Cluster ID or error. If it already exists it will return the current cluster ID. Will return an error if the cluster is
	// currently active (ie. Not disconnected).
	ProvisionOrClaimVizier(context.Context, uuid.UUID, uuid.UUID, string, string) (uuid.UUID, string, error)
}

// Service is the deployment service.
type Service struct {
	deploymentInfoFetcher InfoFetcher
	vp                    VizierProvisioner
}

// New creates a deployment service.
func New(dif InfoFetcher, vp VizierProvisioner) *Service {
	return &Service{deploymentInfoFetcher: dif, vp: vp}
}

// RegisterVizierDeployment will use the deployment key to generate or fetch the vizier key.
func (s *Service) RegisterVizierDeployment(ctx context.Context, req *vzmgrpb.RegisterVizierDeploymentRequest) (*vzmgrpb.RegisterVizierDeploymentResponse, error) {
	if len(req.K8sClusterUID) == 0 {
		return nil, status.Error(codes.InvalidArgument, "empty cluster UID is not allowed")
	}
	// Fetch the orgID and userID based on the deployment key.
	orgID, userID, keyID, err := s.deploymentInfoFetcher.FetchOrgUserIDUsingDeploymentKey(ctx, req.DeploymentKey)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, "invalid/unknown deployment key")
	}
	// Now we know the org and user ID to use for deployment. The process is as follows:
	// 1. Try to fetch a cluster with either an empty UID or one where the UID matches the one in the protobuf.
	// 2. If the UID matches then return that cluster.
	// 3. Otherwise, pick a cluster with no UID specified and claim it.
	// 4. If no empty clusters exist then we create a new cluster.
	clusterID, clusterName, err := s.vp.ProvisionOrClaimVizier(ctx, orgID, userID, req.K8sClusterUID, req.K8sClusterName)
	if err != nil {
		return nil, vzerrors.ToGRPCError(err)
	}

	log.WithField("orgID", orgID).WithField("keyID", keyID).WithField("clusterID", clusterID).WithField("clusterName", clusterName).Info("Successfully registered Vizier deployment")

	return &vzmgrpb.RegisterVizierDeploymentResponse{
		VizierID:   utils.ProtoFromUUID(clusterID),
		VizierName: clusterName,
	}, nil
}
