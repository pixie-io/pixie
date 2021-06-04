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

	"px.dev/pixie/src/api/proto/cloudpb"
)

// CLIArtifactResolver is the resolver responsible for resolving the CLI artifact.
type CLIArtifactResolver struct {
	URL    string
	SHA256 string
}

type cliArtifactArgs struct {
	ArtifactType string
}

func artifactTypeToProto(a string) cloudpb.ArtifactType {
	switch a {
	case "AT_LINUX_AMD64":
		return cloudpb.AT_LINUX_AMD64
	case "AT_DARWIN_AMD64":
		return cloudpb.AT_DARWIN_AMD64
	case "AT_CONTAINER_SET_YAMLS":
		return cloudpb.AT_CONTAINER_SET_YAMLS
	case "AT_CONTAINER_SET_LINUX_AMD64":
		return cloudpb.AT_CONTAINER_SET_LINUX_AMD64
	case "AT_CONTAINER_SET_TEMPLATE_YAMLS":
		return cloudpb.AT_CONTAINER_SET_TEMPLATE_YAMLS
	default:
		return cloudpb.AT_UNKNOWN
	}
}

// CLIArtifact resolves CLI information.
func (q *QueryResolver) CLIArtifact(ctx context.Context, args *cliArtifactArgs) (*CLIArtifactResolver, error) {
	grpcAPI := q.Env.ArtifactTrackerServer

	artifactTypePb := artifactTypeToProto(args.ArtifactType)

	artifactReq := &cloudpb.GetArtifactListRequest{
		ArtifactType: artifactTypePb,
		ArtifactName: "cli",
		Limit:        1,
	}

	cliInfo, err := grpcAPI.GetArtifactList(ctx, artifactReq)
	if err != nil {
		return nil, err
	}

	if len(cliInfo.Artifact) == 0 {
		return nil, fmt.Errorf("No artifact exists")
	}
	if len(cliInfo.Artifact) > 1 {
		return nil, fmt.Errorf("Got unexpected number of artifacts: %d", len(cliInfo.Artifact))
	}

	linkReq := &cloudpb.GetDownloadLinkRequest{
		ArtifactName: "cli",
		ArtifactType: artifactTypePb,
		VersionStr:   cliInfo.Artifact[0].VersionStr,
	}

	linkResp, err := grpcAPI.GetDownloadLink(ctx, linkReq)
	if err != nil {
		return nil, err
	}

	return &CLIArtifactResolver{
		linkResp.Url, linkResp.SHA256,
	}, nil
}

type artifactsArgs struct {
	ArtifactName string
}

// ArtifactResolver is a resolver for a single artifact.
type ArtifactResolver struct {
	TimestampMs float64
	Version     string
	Changelog   string
}

// ArtifactsInfoResolver is a resolver for a list of artifacts.
type ArtifactsInfoResolver struct {
	Items []ArtifactResolver
}

// Artifacts is the resolver responsible for fetching all artifacts.
func (q *QueryResolver) Artifacts(ctx context.Context, args *artifactsArgs) (*ArtifactsInfoResolver, error) {
	grpcAPI := q.Env.ArtifactTrackerServer

	artifactType := cloudpb.AT_LINUX_AMD64
	if args.ArtifactName == "vizier" {
		artifactType = cloudpb.AT_CONTAINER_SET_LINUX_AMD64
	}

	artifactReq := &cloudpb.GetArtifactListRequest{
		ArtifactType: artifactType,
		ArtifactName: args.ArtifactName,
	}

	resp, err := grpcAPI.GetArtifactList(ctx, artifactReq)
	if err != nil {
		return nil, err
	}

	artifacts := make([]ArtifactResolver, len(resp.Artifact))
	for i, a := range resp.Artifact {
		ts := a.Timestamp.Seconds * 1000
		artifacts[i] = ArtifactResolver{
			Version:     a.VersionStr,
			Changelog:   a.Changelog,
			TimestampMs: float64(ts),
		}
	}
	return &ArtifactsInfoResolver{
		Items: artifacts,
	}, nil
}
