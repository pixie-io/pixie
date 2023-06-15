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

	"github.com/spf13/viper"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/shared/artifacts/versionspb"
	srvutils "px.dev/pixie/src/shared/services/utils"
)

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
		availableArtifactMirrors := make([]*cloudpb.ArtifactMirrors, len(artifact.AvailableArtifactMirrors))
		for j, am := range artifact.AvailableArtifactMirrors {
			availableArtifactMirrors[j] = &cloudpb.ArtifactMirrors{
				ArtifactType: getArtifactTypeFromVersionsProto(am.ArtifactType),
				SHA256:       am.SHA256,
				URLs:         am.URLs,
			}
		}
		cloudpbArtifacts[i] = &cloudpb.Artifact{
			Timestamp:                artifact.Timestamp,
			CommitHash:               artifact.CommitHash,
			VersionStr:               artifact.VersionStr,
			Changelog:                artifact.Changelog,
			AvailableArtifacts:       availableArtifacts,
			AvailableArtifactMirrors: availableArtifactMirrors,
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
