package deployment

import (
	"context"

	"github.com/gofrs/uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzerrors"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/utils"
)

// InfoFetcher fetches information about deployments using the key.
type InfoFetcher interface {
	FetchOrgUserIDUsingDeploymentKey(context.Context, string) (uuid.UUID, uuid.UUID, error)
}

// VizierProvisioner provisions a new Vizier.
type VizierProvisioner interface {
	// ProvisionVizier creates the vizier, with specified org_id, user_id, cluster_uid. Returns
	// Cluster ID or error. If it already exists it will return the current cluster ID. Will return an error if the cluster is
	// currently active (ie. Not disconnected).
	ProvisionOrClaimVizier(context.Context, uuid.UUID, uuid.UUID, string, string, string) (uuid.UUID, error)
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
	orgID, userID, err := s.deploymentInfoFetcher.FetchOrgUserIDUsingDeploymentKey(ctx, req.DeploymentKey)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, "invalid/unknown deployment key")
	}
	// Now we know the org and user ID to use for deployment. The process is as follows:
	// 1. Try to fetch a cluster with either an empty UID or one where the UID matches the one in the protobuf.
	// 2. If the UID matches then return that cluster.
	// 3. Otherwise, pick a cluster with no UID specified and claim it.
	// 4. If no empty clusters exist then we create a new cluster.
	clusterID, err := s.vp.ProvisionOrClaimVizier(ctx, orgID, userID, req.K8sClusterUID, req.K8sClusterName, req.K8sClusterVersion)
	if err != nil {
		return nil, vzerrors.ToGRPCError(err)

	}
	return &vzmgrpb.RegisterVizierDeploymentResponse{VizierID: utils.ProtoFromUUID(clusterID)}, nil
}
