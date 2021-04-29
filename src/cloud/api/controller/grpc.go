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

package controller

import (
	"context"
	"fmt"
	"io/ioutil"
	"path/filepath"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/autocomplete"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/cloud/scriptmgr/scriptmgrpb"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/artifacts/versionspb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/shared/services/authcontext"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

func init() {
	pflag.String("vizier_image_secret_path", "/vizier-image-secret", "[WORKAROUND] The path the the image secrets")
	pflag.String("vizier_image_secret_file", "vizier_image_secret.json", "[WORKAROUND] The image secret file")
}

// VizierImageAuthServer is the GRPC server responsible for providing access to Vizier images.
type VizierImageAuthServer struct{}

// GetImageCredentials fetches image credentials for vizier.
func (v VizierImageAuthServer) GetImageCredentials(context.Context, *cloudpb.GetImageCredentialsRequest) (*cloudpb.GetImageCredentialsResponse, error) {
	// These creds are just for internal use. All official releases are written to a public registry.
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

	return &cloudpb.GetImageCredentialsResponse{Creds: string(b)}, nil
}

// ArtifactTrackerServer is the GRPC server responsible for providing access to artifacts.
type ArtifactTrackerServer struct {
	ArtifactTrackerClient artifacttrackerpb.ArtifactTrackerClient
}

func getArtifactTypeFromCloudProto(a cloudpb.ArtifactType) versionspb.ArtifactType {
	switch a {
	case cloudpb.AT_LINUX_AMD64:
		return versionspb.AT_LINUX_AMD64
	case cloudpb.AT_DARWIN_AMD64:
		return versionspb.AT_DARWIN_AMD64
	case cloudpb.AT_CONTAINER_SET_YAMLS:
		return versionspb.AT_CONTAINER_SET_YAMLS
	case cloudpb.AT_CONTAINER_SET_LINUX_AMD64:
		return versionspb.AT_CONTAINER_SET_LINUX_AMD64
	case cloudpb.AT_CONTAINER_SET_TEMPLATE_YAMLS:
		return versionspb.AT_CONTAINER_SET_TEMPLATE_YAMLS
	default:
		return versionspb.AT_UNKNOWN
	}
}

func getArtifactTypeFromVersionsProto(a versionspb.ArtifactType) cloudpb.ArtifactType {
	switch a {
	case versionspb.AT_LINUX_AMD64:
		return cloudpb.AT_LINUX_AMD64
	case versionspb.AT_DARWIN_AMD64:
		return cloudpb.AT_DARWIN_AMD64
	case versionspb.AT_CONTAINER_SET_YAMLS:
		return cloudpb.AT_CONTAINER_SET_YAMLS
	case versionspb.AT_CONTAINER_SET_LINUX_AMD64:
		return cloudpb.AT_CONTAINER_SET_LINUX_AMD64
	case versionspb.AT_CONTAINER_SET_TEMPLATE_YAMLS:
		return cloudpb.AT_CONTAINER_SET_TEMPLATE_YAMLS
	default:
		return cloudpb.AT_UNKNOWN
	}
}

func getServiceCredentials(signingKey string) (string, error) {
	claims := srvutils.GenerateJWTForService("API Service", viper.GetString("domain_name"))
	return srvutils.SignJWTClaims(claims, signingKey)
}

// GetArtifactList gets the set of artifact versions for the given artifact.
func (a ArtifactTrackerServer) GetArtifactList(ctx context.Context, req *cloudpb.GetArtifactListRequest) (*cloudpb.ArtifactSet, error) {
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

	cloudpbArtifacts := make([]*cloudpb.Artifact, len(resp.Artifact))
	for i, artifact := range resp.Artifact {
		availableArtifacts := make([]cloudpb.ArtifactType, len(artifact.AvailableArtifacts))
		for j, a := range artifact.AvailableArtifacts {
			availableArtifacts[j] = getArtifactTypeFromVersionsProto(a)
		}
		cloudpbArtifacts[i] = &cloudpb.Artifact{
			Timestamp:          artifact.Timestamp,
			CommitHash:         artifact.CommitHash,
			VersionStr:         artifact.VersionStr,
			Changelog:          artifact.Changelog,
			AvailableArtifacts: availableArtifacts,
		}
	}

	return &cloudpb.ArtifactSet{
		Name:     resp.Name,
		Artifact: cloudpbArtifacts,
	}, nil
}

// GetDownloadLink gets the download link for the given artifact.
func (a ArtifactTrackerServer) GetDownloadLink(ctx context.Context, req *cloudpb.GetDownloadLinkRequest) (*cloudpb.GetDownloadLinkResponse, error) {
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

	return &cloudpb.GetDownloadLinkResponse{
		Url:        resp.Url,
		SHA256:     resp.SHA256,
		ValidUntil: resp.ValidUntil,
	}, nil
}

// VizierClusterInfo is the server that implements the VizierClusterInfo gRPC service.
type VizierClusterInfo struct {
	VzMgr                 vzmgrpb.VZMgrServiceClient
	ArtifactTrackerClient artifacttrackerpb.ArtifactTrackerClient
}

func contextWithAuthToken(ctx context.Context) (context.Context, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	return metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", sCtx.AuthToken)), nil
}

// CreateCluster creates a cluster for the current org.
func (v *VizierClusterInfo) CreateCluster(ctx context.Context, request *cloudpb.CreateClusterRequest) (*cloudpb.CreateClusterResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "Deprecated. Please use `px deploy`")
}

// GetClusterInfo returns information about Vizier clusters.
func (v *VizierClusterInfo) GetClusterInfo(ctx context.Context, request *cloudpb.GetClusterInfoRequest) (*cloudpb.GetClusterInfoResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID, err := uuid.FromString(orgIDstr)
	if err != nil {
		return nil, err
	}

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	vzIDs := make([]*uuidpb.UUID, 0)
	if request.ID != nil {
		vzIDs = append(vzIDs, request.ID)
	} else {
		viziers, err := v.VzMgr.GetViziersByOrg(ctx, utils.ProtoFromUUID(orgID))
		if err != nil {
			return nil, err
		}
		vzIDs = viziers.VizierIDs
	}

	return v.getClusterInfoForViziers(ctx, vzIDs)
}

func convertContainerState(cs metadatapb.ContainerState) cloudpb.ContainerState {
	switch cs {
	case metadatapb.CONTAINER_STATE_RUNNING:
		return cloudpb.CONTAINER_STATE_RUNNING
	case metadatapb.CONTAINER_STATE_TERMINATED:
		return cloudpb.CONTAINER_STATE_TERMINATED
	case metadatapb.CONTAINER_STATE_WAITING:
		return cloudpb.CONTAINER_STATE_WAITING
	case metadatapb.CONTAINER_STATE_UNKNOWN:
		return cloudpb.CONTAINER_STATE_UNKNOWN
	default:
		return cloudpb.CONTAINER_STATE_UNKNOWN
	}
}

func convertPodPhase(p metadatapb.PodPhase) cloudpb.PodPhase {
	switch p {
	case metadatapb.PENDING:
		return cloudpb.PENDING
	case metadatapb.RUNNING:
		return cloudpb.RUNNING
	case metadatapb.SUCCEEDED:
		return cloudpb.SUCCEEDED
	case metadatapb.FAILED:
		return cloudpb.FAILED
	case metadatapb.PHASE_UNKNOWN:
		return cloudpb.PHASE_UNKNOWN
	default:
		return cloudpb.PHASE_UNKNOWN
	}
}

func (v *VizierClusterInfo) getClusterInfoForViziers(ctx context.Context, ids []*uuidpb.UUID) (*cloudpb.GetClusterInfoResponse, error) {
	resp := &cloudpb.GetClusterInfoResponse{}

	cNames := make(map[string]int)
	vzInfoResp, err := v.VzMgr.GetVizierInfos(ctx, &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: ids,
	})

	if err != nil {
		return nil, err
	}

	for _, vzInfo := range vzInfoResp.VizierInfos {
		if vzInfo == nil || vzInfo.VizierID == nil {
			continue
		}
		podStatuses := make(map[string]*cloudpb.PodStatus)
		for podName, status := range vzInfo.ControlPlanePodStatuses {
			var containers []*cloudpb.ContainerStatus
			for _, container := range status.Containers {
				containers = append(containers, &cloudpb.ContainerStatus{
					Name:      container.Name,
					State:     convertContainerState(container.State),
					Message:   container.Message,
					Reason:    container.Reason,
					CreatedAt: container.CreatedAt,
				})
			}
			var events []*cloudpb.K8SEvent
			for _, ev := range status.Events {
				events = append(events, &cloudpb.K8SEvent{
					Message:   ev.Message,
					LastTime:  ev.LastTime,
					FirstTime: ev.FirstTime,
				})
			}

			podStatuses[podName] = &cloudpb.PodStatus{
				Name:          status.Name,
				Status:        convertPodPhase(status.Status),
				StatusMessage: status.StatusMessage,
				Reason:        status.Reason,
				Containers:    containers,
				CreatedAt:     status.CreatedAt,
				Events:        events,
			}
		}

		s := vzStatusToClusterStatus(vzInfo.Status)
		prettyName := PrettifyClusterName(vzInfo.ClusterName, false)

		if val, ok := cNames[prettyName]; ok {
			cNames[prettyName] = val + 1
		} else {
			cNames[prettyName] = 1
		}

		resp.Clusters = append(resp.Clusters, &cloudpb.ClusterInfo{
			ID:              vzInfo.VizierID,
			Status:          s,
			LastHeartbeatNs: vzInfo.LastHeartbeatNs,
			Config: &cloudpb.VizierConfig{
				PassthroughEnabled: vzInfo.Config.PassthroughEnabled,
				AutoUpdateEnabled:  vzInfo.Config.AutoUpdateEnabled,
			},
			ClusterUID:              vzInfo.ClusterUID,
			ClusterName:             vzInfo.ClusterName,
			PrettyClusterName:       prettyName,
			ClusterVersion:          vzInfo.ClusterVersion,
			VizierVersion:           vzInfo.VizierVersion,
			ControlPlanePodStatuses: podStatuses,
			NumNodes:                vzInfo.NumNodes,
			NumInstrumentedNodes:    vzInfo.NumInstrumentedNodes,
		})
	}

	// For duplicate prettyNames, update the prettyNames to have more context.
	for i, c := range resp.Clusters {
		if cNames[c.PrettyClusterName] > 1 {
			resp.Clusters[i].PrettyClusterName = PrettifyClusterName(c.ClusterName, true)
		}
	}

	return resp, nil
}

// GetClusterConnectionInfo returns information about connections to Vizier cluster.
func (v *VizierClusterInfo) GetClusterConnectionInfo(ctx context.Context, request *cloudpb.GetClusterConnectionInfoRequest) (*cloudpb.GetClusterConnectionInfoResponse, error) {
	id := request.ID
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	ci, err := v.VzMgr.GetVizierConnectionInfo(ctx, id)
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetClusterConnectionInfoResponse{
		IPAddress: ci.IPAddress,
		Token:     ci.Token,
	}, nil
}

// UpdateClusterVizierConfig supports updates of VizierConfig for a cluster
func (v *VizierClusterInfo) UpdateClusterVizierConfig(ctx context.Context, req *cloudpb.UpdateClusterVizierConfigRequest) (*cloudpb.UpdateClusterVizierConfigResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	_, err = v.VzMgr.UpdateVizierConfig(ctx, &cvmsgspb.UpdateVizierConfigRequest{
		VizierID: req.ID,
		ConfigUpdate: &cvmsgspb.VizierConfigUpdate{
			PassthroughEnabled: req.ConfigUpdate.PassthroughEnabled,
			AutoUpdateEnabled:  req.ConfigUpdate.AutoUpdateEnabled,
		},
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.UpdateClusterVizierConfigResponse{}, nil
}

// UpdateOrInstallCluster updates or installs the given vizier cluster to the specified version.
func (v *VizierClusterInfo) UpdateOrInstallCluster(ctx context.Context, req *cloudpb.UpdateOrInstallClusterRequest) (*cloudpb.UpdateOrInstallClusterResponse, error) {
	if req.Version == "" {
		return nil, status.Errorf(codes.InvalidArgument, "version cannot be empty")
	}

	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	// Validate version.
	atReq := &artifacttrackerpb.GetDownloadLinkRequest{
		ArtifactName: "vizier",
		VersionStr:   req.Version,
		ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
	}

	_, err = v.ArtifactTrackerClient.GetDownloadLink(ctx, atReq)
	if err != nil {
		return nil, status.Errorf(codes.InvalidArgument, "invalid version")
	}

	resp, err := v.VzMgr.UpdateOrInstallVizier(ctx, &cvmsgspb.UpdateOrInstallVizierRequest{
		VizierID:     req.ClusterID,
		Version:      req.Version,
		RedeployEtcd: req.RedeployEtcd,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.UpdateOrInstallClusterResponse{
		UpdateStarted: resp.UpdateStarted,
	}, nil
}

func vzStatusToClusterStatus(s cvmsgspb.VizierStatus) cloudpb.ClusterStatus {
	switch s {
	case cvmsgspb.VZ_ST_HEALTHY:
		return cloudpb.CS_HEALTHY
	case cvmsgspb.VZ_ST_UNHEALTHY:
		return cloudpb.CS_UNHEALTHY
	case cvmsgspb.VZ_ST_DISCONNECTED:
		return cloudpb.CS_DISCONNECTED
	case cvmsgspb.VZ_ST_UPDATING:
		return cloudpb.CS_UPDATING
	case cvmsgspb.VZ_ST_CONNECTED:
		return cloudpb.CS_CONNECTED
	case cvmsgspb.VZ_ST_UPDATE_FAILED:
		return cloudpb.CS_UPDATE_FAILED
	default:
		return cloudpb.CS_UNKNOWN
	}
}

// VizierDeploymentKeyServer is the server that implements the VizierDeploymentKeyManager gRPC service.
type VizierDeploymentKeyServer struct {
	VzDeploymentKey vzmgrpb.VZDeploymentKeyServiceClient
}

func deployKeyToCloudAPI(key *vzmgrpb.DeploymentKey) *cloudpb.DeploymentKey {
	return &cloudpb.DeploymentKey{
		ID:        key.ID,
		Key:       key.Key,
		CreatedAt: key.CreatedAt,
		Desc:      key.Desc,
	}
}

// Create creates a new deploy key in vzmgr.
func (v *VizierDeploymentKeyServer) Create(ctx context.Context, req *cloudpb.CreateDeploymentKeyRequest) (*cloudpb.DeploymentKey, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.VzDeploymentKey.Create(ctx, &vzmgrpb.CreateDeploymentKeyRequest{Desc: req.Desc})
	if err != nil {
		return nil, err
	}
	return deployKeyToCloudAPI(resp), nil
}

// List lists all of the deploy keys in vzmgr.
func (v *VizierDeploymentKeyServer) List(ctx context.Context, req *cloudpb.ListDeploymentKeyRequest) (*cloudpb.ListDeploymentKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.VzDeploymentKey.List(ctx, &vzmgrpb.ListDeploymentKeyRequest{})
	if err != nil {
		return nil, err
	}
	var keys []*cloudpb.DeploymentKey
	for _, key := range resp.Keys {
		keys = append(keys, deployKeyToCloudAPI(key))
	}
	return &cloudpb.ListDeploymentKeyResponse{
		Keys: keys,
	}, nil
}

// Get fetches a specific deploy key in vzmgr.
func (v *VizierDeploymentKeyServer) Get(ctx context.Context, req *cloudpb.GetDeploymentKeyRequest) (*cloudpb.GetDeploymentKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.VzDeploymentKey.Get(ctx, &vzmgrpb.GetDeploymentKeyRequest{
		ID: req.ID,
	})
	if err != nil {
		return nil, err
	}
	return &cloudpb.GetDeploymentKeyResponse{
		Key: deployKeyToCloudAPI(resp.Key),
	}, nil
}

// Delete deletes a specific deploy key in vzmgr.
func (v *VizierDeploymentKeyServer) Delete(ctx context.Context, uuid *uuidpb.UUID) (*types.Empty, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	return v.VzDeploymentKey.Delete(ctx, uuid)
}

// APIKeyServer is the server that implements the APIKeyManager gRPC service.
type APIKeyServer struct {
	APIKeyClient authpb.APIKeyServiceClient
}

func apiKeyToCloudAPI(key *authpb.APIKey) *cloudpb.APIKey {
	return &cloudpb.APIKey{
		ID:        key.ID,
		Key:       key.Key,
		CreatedAt: key.CreatedAt,
		Desc:      key.Desc,
	}
}

// Create creates a new API key.
func (v *APIKeyServer) Create(ctx context.Context, req *cloudpb.CreateAPIKeyRequest) (*cloudpb.APIKey, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.APIKeyClient.Create(ctx, &authpb.CreateAPIKeyRequest{Desc: req.Desc})
	if err != nil {
		return nil, err
	}
	return apiKeyToCloudAPI(resp), nil
}

// List lists all of the API keys in vzmgr.
func (v *APIKeyServer) List(ctx context.Context, req *cloudpb.ListAPIKeyRequest) (*cloudpb.ListAPIKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.APIKeyClient.List(ctx, &authpb.ListAPIKeyRequest{})
	if err != nil {
		return nil, err
	}
	var keys []*cloudpb.APIKey
	for _, key := range resp.Keys {
		keys = append(keys, apiKeyToCloudAPI(key))
	}
	return &cloudpb.ListAPIKeyResponse{
		Keys: keys,
	}, nil
}

// Get fetches a specific API key.
func (v *APIKeyServer) Get(ctx context.Context, req *cloudpb.GetAPIKeyRequest) (*cloudpb.GetAPIKeyResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := v.APIKeyClient.Get(ctx, &authpb.GetAPIKeyRequest{
		ID: req.ID,
	})
	if err != nil {
		return nil, err
	}
	return &cloudpb.GetAPIKeyResponse{
		Key: apiKeyToCloudAPI(resp.Key),
	}, nil
}

// Delete deletes a specific API key.
func (v *APIKeyServer) Delete(ctx context.Context, uuid *uuidpb.UUID) (*types.Empty, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	return v.APIKeyClient.Delete(ctx, uuid)
}

// AutocompleteServer is the server that implements the Autocomplete gRPC service.
type AutocompleteServer struct {
	Suggester autocomplete.Suggester
}

// Autocomplete returns a formatted string and autocomplete suggestions.
func (a *AutocompleteServer) Autocomplete(ctx context.Context, req *cloudpb.AutocompleteRequest) (*cloudpb.AutocompleteResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID, err := uuid.FromString(orgIDstr)
	if err != nil {
		return nil, err
	}

	fmtString, executable, suggestions, err := autocomplete.Autocomplete(req.Input, int(req.CursorPos), req.Action, a.Suggester, orgID, req.ClusterUID)
	if err != nil {
		return nil, err
	}

	return &cloudpb.AutocompleteResponse{
		FormattedInput: fmtString,
		IsExecutable:   executable,
		TabSuggestions: suggestions,
	}, nil
}

// AutocompleteField returns suggestions for a single field.
func (a *AutocompleteServer) AutocompleteField(ctx context.Context, req *cloudpb.AutocompleteFieldRequest) (*cloudpb.AutocompleteFieldResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID, err := uuid.FromString(orgIDstr)
	if err != nil {
		return nil, err
	}

	allowedArgs := []cloudpb.AutocompleteEntityKind{}
	if req.RequiredArgTypes != nil {
		allowedArgs = req.RequiredArgTypes
	}

	suggestionReq := []*autocomplete.SuggestionRequest{
		{
			OrgID:        orgID,
			Input:        req.Input,
			AllowedKinds: []cloudpb.AutocompleteEntityKind{req.FieldType},
			AllowedArgs:  allowedArgs,
			ClusterUID:   req.ClusterUID,
		},
	}
	suggestions, err := a.Suggester.GetSuggestions(suggestionReq)
	if err != nil {
		return nil, err
	}
	if len(suggestions) != 1 {
		return nil, status.Error(codes.Internal, "failed to get autocomplete suggestions")
	}

	acSugg := make([]*cloudpb.AutocompleteSuggestion, len(suggestions[0].Suggestions))
	for j, s := range suggestions[0].Suggestions {
		acSugg[j] = &cloudpb.AutocompleteSuggestion{
			Kind:           s.Kind,
			Name:           s.Name,
			Description:    s.Desc,
			MatchedIndexes: s.MatchedIndexes,
			State:          s.State,
		}
	}

	return &cloudpb.AutocompleteFieldResponse{
		Suggestions: acSugg,
	}, nil
}

// ScriptMgrServer is the server that implements the ScriptMgr gRPC service.
type ScriptMgrServer struct {
	ScriptMgr scriptmgrpb.ScriptMgrServiceClient
}

// GetLiveViews returns a list of all available live views.
func (s *ScriptMgrServer) GetLiveViews(ctx context.Context, req *cloudpb.GetLiveViewsReq) (*cloudpb.GetLiveViewsResp, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}
	smReq := &scriptmgrpb.GetLiveViewsReq{}
	smResp, err := s.ScriptMgr.GetLiveViews(ctx, smReq)
	if err != nil {
		return nil, err
	}
	resp := &cloudpb.GetLiveViewsResp{
		LiveViews: make([]*cloudpb.LiveViewMetadata, len(smResp.LiveViews)),
	}
	for i, liveView := range smResp.LiveViews {
		resp.LiveViews[i] = &cloudpb.LiveViewMetadata{
			ID:   utils.UUIDFromProtoOrNil(liveView.ID).String(),
			Name: liveView.Name,
			Desc: liveView.Desc,
		}
	}
	return resp, nil
}

// GetLiveViewContents returns the pxl script, vis info, and metdata for a live view.
func (s *ScriptMgrServer) GetLiveViewContents(ctx context.Context, req *cloudpb.GetLiveViewContentsReq) (*cloudpb.GetLiveViewContentsResp, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	smReq := &scriptmgrpb.GetLiveViewContentsReq{
		LiveViewID: utils.ProtoFromUUIDStrOrNil(req.LiveViewID),
	}
	smResp, err := s.ScriptMgr.GetLiveViewContents(ctx, smReq)
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetLiveViewContentsResp{
		Metadata: &cloudpb.LiveViewMetadata{
			ID:   req.LiveViewID,
			Name: smResp.Metadata.Name,
			Desc: smResp.Metadata.Desc,
		},
		PxlContents: smResp.PxlContents,
		Vis:         smResp.Vis,
	}, nil
}

// GetScripts returns a list of all available scripts.
func (s *ScriptMgrServer) GetScripts(ctx context.Context, req *cloudpb.GetScriptsReq) (*cloudpb.GetScriptsResp, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	smReq := &scriptmgrpb.GetScriptsReq{}
	smResp, err := s.ScriptMgr.GetScripts(ctx, smReq)
	if err != nil {
		return nil, err
	}
	resp := &cloudpb.GetScriptsResp{
		Scripts: make([]*cloudpb.ScriptMetadata, len(smResp.Scripts)),
	}
	for i, script := range smResp.Scripts {
		resp.Scripts[i] = &cloudpb.ScriptMetadata{
			ID:          utils.UUIDFromProtoOrNil(script.ID).String(),
			Name:        script.Name,
			Desc:        script.Desc,
			HasLiveView: script.HasLiveView,
		}
	}
	return resp, nil
}

// GetScriptContents returns the pxl string of the script.
func (s *ScriptMgrServer) GetScriptContents(ctx context.Context, req *cloudpb.GetScriptContentsReq) (*cloudpb.GetScriptContentsResp, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	smReq := &scriptmgrpb.GetScriptContentsReq{
		ScriptID: utils.ProtoFromUUIDStrOrNil(req.ScriptID),
	}
	smResp, err := s.ScriptMgr.GetScriptContents(ctx, smReq)
	if err != nil {
		return nil, err
	}
	return &cloudpb.GetScriptContentsResp{
		Metadata: &cloudpb.ScriptMetadata{
			ID:          req.ScriptID,
			Name:        smResp.Metadata.Name,
			Desc:        smResp.Metadata.Desc,
			HasLiveView: smResp.Metadata.HasLiveView,
		},
		Contents: smResp.Contents,
	}, nil
}

// ProfileServer provides info about users and orgs.
type ProfileServer struct {
	ProfileServiceClient profilepb.ProfileServiceClient
}

// GetOrgInfo gets the org info for a given org ID.
func (p *ProfileServer) GetOrgInfo(ctx context.Context, req *uuidpb.UUID) (*cloudpb.OrgInfo, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	claimsOrgID := sCtx.Claims.GetUserClaims().OrgID
	orgID := utils.UUIDFromProtoOrNil(req)
	if claimsOrgID != orgID.String() {
		return nil, status.Error(codes.Unauthenticated, "Unable to fetch org info")
	}

	resp, err := p.ProfileServiceClient.GetOrg(ctx, req)
	if err != nil {
		return nil, err
	}

	return &cloudpb.OrgInfo{
		ID:      resp.ID,
		OrgName: resp.OrgName,
	}, nil
}

// InviteUser creates and returns an invite link for the org for the specified user info.
func (p *ProfileServer) InviteUser(ctx context.Context, externalReq *cloudpb.InviteUserRequest) (*cloudpb.InviteUserResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	claimsOrgID := sCtx.Claims.GetUserClaims().OrgID
	orgIDPb := utils.ProtoFromUUIDStrOrNil(claimsOrgID)
	if orgIDPb == nil {
		return nil, status.Errorf(codes.InvalidArgument, "Could not identify user's org")
	}

	internalReq := &profilepb.InviteUserRequest{
		OrgID:          orgIDPb,
		Email:          externalReq.Email,
		FirstName:      externalReq.FirstName,
		LastName:       externalReq.LastName,
		MustCreateUser: true,
	}
	resp, err := p.ProfileServiceClient.InviteUser(ctx, internalReq)
	if err != nil {
		return nil, err
	}

	return &cloudpb.InviteUserResponse{
		Email:      resp.Email,
		InviteLink: resp.InviteLink,
	}, nil
}
