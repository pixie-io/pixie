package controller

import (
	"context"
	"fmt"
	"io/ioutil"
	"path/filepath"

	uuid "github.com/satori/go.uuid"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	pbutils "pixielabs.ai/pixielabs/src/utils"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"github.com/spf13/pflag"
	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	versionspb "pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
)

func init() {
	pflag.String("vizier_image_secret_path", "/vizier-image-secret", "[WORKAROUND] The path the the image secrets")
	pflag.String("vizier_image_secret_file", "vizier_image_secret.json", "[WORKAROUND] The image secret file")
}

// VizierImageAuthSever is the GRPC server responsible for providing access to Vizier images.
type VizierImageAuthSever struct{}

// GetImageCredentials fetches image credentials for vizier.
func (v VizierImageAuthSever) GetImageCredentials(context.Context, *cloudapipb.GetImageCredentialsRequest) (*cloudapipb.GetImageCredentialsResponse, error) {
	// TODO(zasgar/michelle): Fix this to create creds for user.
	// This is a workaround implementation to just give them access based on static keys.
	p := viper.GetString("vizier_image_secret_path")
	f := viper.GetString("vizier_image_secret_file")

	absP, err := filepath.Abs(p)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to parse creds paths")
	}
	credsFile := filepath.Join(absP, f)
	b, err := ioutil.ReadFile(credsFile)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to read creds file")
	}

	return &cloudapipb.GetImageCredentialsResponse{Creds: string(b)}, nil
}

// ArtifactTrackerServer is the GRPC server responsible for providing access to artifacts.
type ArtifactTrackerServer struct {
	ArtifactTrackerClient artifacttrackerpb.ArtifactTrackerClient
}

func getArtifactTypeFromCloudProto(a cloudapipb.ArtifactType) versionspb.ArtifactType {
	switch a {
	case cloudapipb.AT_LINUX_AMD64:
		return versionspb.AT_LINUX_AMD64
	case cloudapipb.AT_DARWIN_AMD64:
		return versionspb.AT_DARWIN_AMD64
	case cloudapipb.AT_CONTAINER_SET_YAMLS:
		return versionspb.AT_CONTAINER_SET_YAMLS
	case cloudapipb.AT_CONTAINER_SET_LINUX_AMD64:
		return versionspb.AT_CONTAINER_SET_LINUX_AMD64
	default:
		return versionspb.AT_UNKNOWN
	}
}

func getArtifactTypeFromVersionsProto(a versionspb.ArtifactType) cloudapipb.ArtifactType {
	switch a {
	case versionspb.AT_LINUX_AMD64:
		return cloudapipb.AT_LINUX_AMD64
	case versionspb.AT_DARWIN_AMD64:
		return cloudapipb.AT_DARWIN_AMD64
	case versionspb.AT_CONTAINER_SET_YAMLS:
		return cloudapipb.AT_CONTAINER_SET_YAMLS
	case versionspb.AT_CONTAINER_SET_LINUX_AMD64:
		return cloudapipb.AT_CONTAINER_SET_LINUX_AMD64
	default:
		return cloudapipb.AT_UNKNOWN
	}
}

func getServiceCredentials(signingKey string) (string, error) {
	claims := utils.GenerateJWTForService("API Service")
	return utils.SignJWTClaims(claims, signingKey)
}

// GetArtifactList gets the set of artifact versions for the given artifact.
func (a ArtifactTrackerServer) GetArtifactList(ctx context.Context, req *cloudapipb.GetArtifactListRequest) (*cloudapipb.ArtifactSet, error) {
	atReq := &artifacttrackerpb.GetArtifactListRequest{
		ArtifactType: getArtifactTypeFromCloudProto(req.ArtifactType),
		ArtifactName: req.ArtifactName,
		Limit:        req.Limit,
	}

	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	resp, err := a.ArtifactTrackerClient.GetArtifactList(ctx, atReq)
	if err != nil {
		return nil, err
	}

	cloudpbArtifacts := make([]*cloudapipb.Artifact, len(resp.Artifact))
	for i, artifact := range resp.Artifact {
		availableArtifacts := make([]cloudapipb.ArtifactType, len(artifact.AvailableArtifacts))
		for j, a := range artifact.AvailableArtifacts {
			availableArtifacts[j] = getArtifactTypeFromVersionsProto(a)
		}
		cloudpbArtifacts[i] = &cloudapipb.Artifact{
			Timestamp:          artifact.Timestamp,
			CommitHash:         artifact.CommitHash,
			VersionStr:         artifact.VersionStr,
			Changelog:          artifact.Changelog,
			AvailableArtifacts: availableArtifacts,
		}
	}

	return &cloudapipb.ArtifactSet{
		Name:     resp.Name,
		Artifact: cloudpbArtifacts,
	}, nil
}

// GetDownloadLink gets the download link for the given artifact.
func (a ArtifactTrackerServer) GetDownloadLink(ctx context.Context, req *cloudapipb.GetDownloadLinkRequest) (*cloudapipb.GetDownloadLinkResponse, error) {
	atReq := &artifacttrackerpb.GetDownloadLinkRequest{
		ArtifactName: req.ArtifactName,
		VersionStr:   req.VersionStr,
		ArtifactType: getArtifactTypeFromCloudProto(req.ArtifactType),
	}

	serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
	if err != nil {
		return nil, err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	resp, err := a.ArtifactTrackerClient.GetDownloadLink(ctx, atReq)
	if err != nil {
		return nil, err
	}

	return &cloudapipb.GetDownloadLinkResponse{
		Url:        resp.Url,
		SHA256:     resp.SHA256,
		ValidUntil: resp.ValidUntil,
	}, nil
}

// VizierClusterInfoServer is the server that implements the VizierClusterInfo gRPC service.
type VizierClusterInfoServer struct {
	VzMgr vzmgrpb.VZMgrServiceClient
}

// GetClusterInfo returns information about Vizier clusters.
func (v *VizierClusterInfoServer) GetClusterInfo(ctx context.Context, request *cloudapipb.GetClusterInfoRequest) (*cloudapipb.GetClusterInfoResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID, err := uuid.FromString(orgIDstr)
	if err != nil {
		return nil, err
	}

	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", sCtx.AuthToken))

	viziers, err := v.VzMgr.GetViziersByOrg(ctx, pbutils.ProtoFromUUID(&orgID))
	if err != nil {
		return nil, err
	}

	resp := &cloudapipb.GetClusterInfoResponse{}
	for _, id := range viziers.VizierIDs {
		// TODO(zasgar/michelle): Make these requests parallel
		vzInfo, err := v.VzMgr.GetVizierInfo(ctx, id)
		if err != nil {
			return nil, err
		}

		s := vzStatusToClusterStatus(vzInfo.Status)
		resp.Clusters = append(resp.Clusters, &cloudapipb.ClusterInfo{
			ID:              id,
			Status:          s,
			LastHeartbeatNs: vzInfo.LastHeartbeatNs,
			Config: &cloudapipb.VizierConfig{
				PassthroughEnabled: vzInfo.Config.PassthroughEnabled,
			},
		})
	}
	return resp, nil
}

// GetClusterConnectionInfo returns information about connections to Vizier cluster.
func (v *VizierClusterInfoServer) GetClusterConnectionInfo(ctx context.Context, request *cloudapipb.GetClusterConnectionInfoRequest) (*cloudapipb.GetClusterConnectionInfoResponse, error) {
	id := request.ID

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	ctx = metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", sCtx.AuthToken))

	ci, err := v.VzMgr.GetVizierConnectionInfo(ctx, id)
	if err != nil {
		return nil, err
	}

	return &cloudapipb.GetClusterConnectionInfoResponse{
		IPAddress: ci.IPAddress,
		Token:     ci.Token,
	}, nil
}

// UpdateClusterVizierConfig supports updates of VizierConfig for a cluster
func (v *VizierClusterInfoServer) UpdateClusterVizierConfig(ctx context.Context, req *cloudapipb.UpdateClusterVizierConfigRequest) (*cloudapipb.UpdateClusterVizierConfigResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", sCtx.AuthToken))

	_, err = v.VzMgr.UpdateVizierConfig(ctx, &cvmsgspb.UpdateVizierConfigRequest{
		VizierID: req.ID,
		ConfigUpdate: &cvmsgspb.VizierConfigUpdate{
			PassthroughEnabled: req.ConfigUpdate.PassthroughEnabled,
		},
	})
	if err != nil {
		return nil, err
	}

	return &cloudapipb.UpdateClusterVizierConfigResponse{}, nil
}

func vzStatusToClusterStatus(s cvmsgspb.VizierInfo_Status) cloudapipb.ClusterStatus {
	switch s {
	case cvmsgspb.VZ_ST_HEALTHY:
		return cloudapipb.CS_HEALTHY
	case cvmsgspb.VZ_ST_UNHEALTHY:
		return cloudapipb.CS_UNHEALTHY
	case cvmsgspb.VZ_ST_DISCONNECTED:
		return cloudapipb.CS_DISCONNECTED
	default:
		return cloudapipb.CS_UNKNOWN
	}
}
