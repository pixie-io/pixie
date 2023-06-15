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
	"io"
	"path"
	"strings"
	"time"

	"cloud.google.com/go/storage"
	"github.com/gogo/protobuf/types"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"golang.org/x/oauth2/jwt"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	apb "px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/shared/artifacts/manifest"
	vpb "px.dev/pixie/src/shared/artifacts/versionspb"
)

// URLSigner is the function used to sign urls.
var URLSigner = storage.SignedURL

const (
	vizierArtifactName   = "vizier"
	cliArtifactName      = "cli"
	operatorArtifactName = "operator"
)

// Server is the controller for the artifact tracker service.
type Server struct {
	sc             stiface.Client
	artifactBucket string
	gcsSA          *jwt.Config
	m              *manifest.ArtifactManifest
}

// NewServer creates a new artifact tracker server.
func NewServer(client stiface.Client, bucket string, gcsSA *jwt.Config) *Server {
	return &Server{sc: client, artifactBucket: bucket, gcsSA: gcsSA}
}

func (s *Server) getArtifactListSpecifiedVizier() (*vpb.ArtifactSet, error) {
	return &vpb.ArtifactSet{
		Name: vizierArtifactName,
		Artifact: []*vpb.Artifact{
			{
				VersionStr: viper.GetString("vizier_version"),
				AvailableArtifacts: []vpb.ArtifactType{
					vpb.AT_CONTAINER_SET_YAMLS,
					vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS,
					vpb.AT_CONTAINER_SET_LINUX_AMD64,
				},
			},
		},
	}, nil
}

func (s *Server) getArtifactListSpecifiedCLI() (*vpb.ArtifactSet, error) {
	return &vpb.ArtifactSet{
		Name: cliArtifactName,
		Artifact: []*vpb.Artifact{
			{
				VersionStr: viper.GetString("cli_version"),
				AvailableArtifacts: []vpb.ArtifactType{
					vpb.AT_LINUX_AMD64,
					vpb.AT_DARWIN_AMD64,
				},
			},
		},
	}, nil
}

func (s *Server) getArtifactListSpecifiedOperator() (*vpb.ArtifactSet, error) {
	return &vpb.ArtifactSet{
		Name: operatorArtifactName,
		Artifact: []*vpb.Artifact{
			{
				VersionStr: viper.GetString("operator_version"),
				AvailableArtifacts: []vpb.ArtifactType{
					vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS,
					vpb.AT_CONTAINER_SET_LINUX_AMD64,
					vpb.AT_CONTAINER_SET_YAMLS,
				},
			},
		},
	}, nil
}

// GetArtifactList returns a list of artifacts matching the passed in criteria.
func (s *Server) GetArtifactList(ctx context.Context, in *apb.GetArtifactListRequest) (*vpb.ArtifactSet, error) {
	name := in.ArtifactName
	at := in.ArtifactType
	limit := in.Limit

	if at == vpb.AT_UNKNOWN {
		return nil, status.Error(codes.InvalidArgument, "artifact type cannot be unknown")
	}

	// If a particular vizier or CLI version is specified, we don't need to make a call to the DB.
	if name == vizierArtifactName && viper.GetString("vizier_version") != "" {
		return s.getArtifactListSpecifiedVizier()
	} else if name == cliArtifactName && viper.GetString("cli_version") != "" {
		return s.getArtifactListSpecifiedCLI()
	} else if name == operatorArtifactName && viper.GetString("operator_version") != "" {
		return s.getArtifactListSpecifiedOperator()
	}

	artifacts, err := s.m.ListArtifacts(name, limit, manifest.RemovePrereleasesFilter(), manifest.ArtifactTypeFilter(at))
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	response := &vpb.ArtifactSet{
		Name:     name,
		Artifact: artifacts,
	}

	return response, nil
}

func downloadSuffix(at vpb.ArtifactType) string {
	switch at {
	case vpb.AT_LINUX_AMD64:
		return "linux_amd64"
	case vpb.AT_DARWIN_AMD64:
		return "darwin_amd64"
	case vpb.AT_CONTAINER_SET_YAMLS:
		return "yamls.tar"
	case vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS:
		return "template_yamls.tar"
	}
	return "unknown"
}

// GetDownloadLink returns a signed download link that can be used to download the artifact.
func (s *Server) GetDownloadLink(ctx context.Context, in *apb.GetDownloadLinkRequest) (*apb.GetDownloadLinkResponse, error) {
	versionStr := in.VersionStr
	name := in.ArtifactName
	at := in.ArtifactType

	if len(name) == 0 {
		return nil, status.Error(codes.InvalidArgument, "name cannot be empty")
	}

	if len(versionStr) == 0 {
		return nil, status.Error(codes.InvalidArgument, "versionStr cannot be empty")
	}

	if at == vpb.AT_UNKNOWN {
		return nil, status.Error(codes.InvalidArgument, "artifact type cannot be unknown")
	}

	if !(at == vpb.AT_DARWIN_AMD64 || at == vpb.AT_LINUX_AMD64 || at == vpb.AT_CONTAINER_SET_YAMLS || at == vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS) {
		return nil, status.Error(codes.InvalidArgument, "artifact type cannot be downloaded")
	}

	// If a specific vizier or CLI version is specified, check that the requested version matches. Otherwise, if no version is specified
	// then we check the DB to see if the version exists.
	if name == vizierArtifactName && (at == vpb.AT_CONTAINER_SET_YAMLS || at == vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS) && viper.GetString("vizier_version") != "" {
		if versionStr != viper.GetString("vizier_version") {
			return nil, status.Error(codes.NotFound, "artifact not found")
		}
	} else if name == operatorArtifactName && (at == vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS) && viper.GetString("operator_version") != "" {
		if versionStr != viper.GetString("operator_version") {
			return nil, status.Error(codes.NotFound, "artifact not found")
		}
	} else if name == cliArtifactName && (at == vpb.AT_DARWIN_AMD64 || at == vpb.AT_LINUX_AMD64) && viper.GetString("cli_version") != "" {
		if versionStr != viper.GetString("cli_version") {
			return nil, status.Error(codes.NotFound, "artifact not found")
		}
	} else {
		a, err := s.m.GetArtifact(name, versionStr)
		if err != nil {
			return nil, status.Error(codes.NotFound, "artifact not found")
		}

		for _, am := range a.AvailableArtifactMirrors {
			if am.ArtifactType == at && len(am.URLs) > 0 {
				return s.getDownloadLinkForMirrors(ctx, am)
			}
		}
		// Fallthrough to the legacy method.

		hasAT := false
		for _, t := range a.AvailableArtifacts {
			if t == at {
				hasAT = true
				break
			}
		}
		if !hasAT {
			return nil, status.Error(codes.NotFound, "artifact with given artifact_type not found")
		}
	}

	expires := time.Now().Add(time.Minute * 60)

	// Artifact found, generate the download link.
	// location: gs://<artifact_bucket>/cli/2019.10.03-1/cli_linux_amd64
	bucket := s.artifactBucket

	objectPath := path.Join(name, versionStr, fmt.Sprintf("%s_%s", name, downloadSuffix(at)))
	attr, err := s.sc.Bucket(bucket).Object(objectPath).Attrs(ctx)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to get URL")
	}
	url := attr.MediaLink

	tpb, _ := types.TimestampProto(expires)

	sha256ObjectPath := objectPath + ".sha256"
	r, err := s.sc.Bucket(bucket).Object(sha256ObjectPath).NewReader(ctx)

	if err != nil {
		return nil, status.Error(codes.Internal, "failed to fetch sha256 file")
	}
	defer r.Close()

	sha256bytes, err := io.ReadAll(r)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to read sha256 file")
	}

	return &apb.GetDownloadLinkResponse{
		Url:        url,
		SHA256:     strings.TrimSpace(string(sha256bytes)),
		ValidUntil: tpb,
	}, nil
}

func (s *Server) getDownloadLinkForMirrors(ctx context.Context, am *vpb.ArtifactMirrors) (*apb.GetDownloadLinkResponse, error) {
	// For now we return a download link to the first mirror.
	// In the future, the API will change to support returning multiple mirrors.
	url := am.URLs[0]
	sha256Bytes := am.SHA256
	valid, err := types.TimestampProto(time.Now().Add(60 * time.Minute))
	if err != nil {
		return nil, err
	}
	return &apb.GetDownloadLinkResponse{
		Url:        url,
		SHA256:     strings.TrimSpace(string(sha256Bytes)),
		ValidUntil: valid,
	}, nil
}

// UpdateManifest switches the server's manifest to use the one given.
func (s *Server) UpdateManifest(m *manifest.ArtifactManifest) error {
	s.m = m
	log.Info("Updated Artifact Manifest")
	return nil
}
