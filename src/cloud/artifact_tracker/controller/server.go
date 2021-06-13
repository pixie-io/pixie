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
	"path"
	"strings"
	"time"

	"cloud.google.com/go/storage"
	"github.com/gogo/protobuf/types"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	"github.com/jmoiron/sqlx"
	"github.com/lib/pq"
	"github.com/spf13/viper"
	"golang.org/x/oauth2/jwt"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	apb "px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	vpb "px.dev/pixie/src/shared/artifacts/versionspb"
	"px.dev/pixie/src/shared/artifacts/versionspb/utils"
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
	db             *sqlx.DB
	sc             stiface.Client
	artifactBucket string
	releaseBucket  string
	gcsSA          *jwt.Config
}

// NewServer creates a new artifact tracker server.
func NewServer(db *sqlx.DB, client stiface.Client, bucket string, releaseBucket string, gcsSA *jwt.Config) *Server {
	return &Server{db: db, sc: client, artifactBucket: bucket, releaseBucket: releaseBucket, gcsSA: gcsSA}
}

func (s *Server) getArtifactListSpecifiedVizier() (*vpb.ArtifactSet, error) {
	return &vpb.ArtifactSet{
		Name: vizierArtifactName,
		Artifact: []*vpb.Artifact{
			&vpb.Artifact{
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
			&vpb.Artifact{
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
			&vpb.Artifact{
				VersionStr: viper.GetString("operator_version"),
				AvailableArtifacts: []vpb.ArtifactType{
					vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS,
					vpb.AT_CONTAINER_SET_LINUX_AMD64,
				},
			},
		},
	}, nil
}

// GetArtifactList returns a list of artifacts matching the passed in criteria.
func (s *Server) GetArtifactList(ctx context.Context, in *apb.GetArtifactListRequest) (*vpb.ArtifactSet, error) {
	name := in.ArtifactName
	at := utils.ToArtifactTypeDB(in.ArtifactType)
	limit := in.Limit

	if at == utils.ATUnknown {
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

	type dbResult struct {
		ArtifactName       string         `db:"artifact_name"`
		CreateTime         time.Time      `db:"create_time"`
		CommitHash         string         `db:"commit_hash"`
		VersionStr         string         `db:"version_str"`
		AvailableArtifacts pq.StringArray `db:"available_artifacts"`
		Changelog          string         `db:"changelog"`
	}

	dbResultToProto := func(res *dbResult) *vpb.Artifact {
		t, _ := types.TimestampProto(res.CreateTime)
		pb := &vpb.Artifact{
			Timestamp:          t,
			CommitHash:         res.CommitHash,
			VersionStr:         res.VersionStr,
			AvailableArtifacts: utils.ToProtoArtifactTypeArray(res.AvailableArtifacts),
			Changelog:          res.Changelog,
		}
		return pb
	}

	query := `SELECT
                artifact_name, create_time, commit_hash, version_str, available_artifacts, changelog
              FROM artifacts, artifact_changelogs
              WHERE artifact_name=$1
                    AND artifact_changelogs.artifacts_id=artifacts.id
                    AND $2=ANY(available_artifacts)
                    -- Pre release builds contain a '-', so we filter those (but still make them available for download)
                    -- The permissions of this should eventually be controlled using an RBAC rule.
                    AND version_str NOT LIKE '%-%'
              ORDER BY create_time DESC`

	var rows *sqlx.Rows
	var err error
	if limit != 0 && limit != -1 {
		query += " LIMIT $3;"
		rows, err = s.db.Queryx(query, name, at, limit)
	} else {
		query += ";"
		rows, err = s.db.Queryx(query, name, at)
	}

	if err != nil {
		return nil, status.Error(codes.Internal, "failed to query database")
	}

	defer rows.Close()

	response := &vpb.ArtifactSet{
		Name:     name,
		Artifact: make([]*vpb.Artifact, 0),
	}

	for rows.Next() {
		res := &dbResult{}
		err = rows.StructScan(res)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to parse database result")
		}
		response.Artifact = append(response.Artifact, dbResultToProto(res))
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
	if (at == vpb.AT_CONTAINER_SET_YAMLS || at == vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS) && viper.GetString("vizier_version") != "" {
		if versionStr != viper.GetString("vizier_version") {
			return nil, status.Error(codes.NotFound, "artifact not found")
		}
	} else if (at == vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS) && viper.GetString("operator_version") != "" {
		if versionStr != viper.GetString("operator_version") {
			return nil, status.Error(codes.NotFound, "artifact not found")
		}
	} else if (at == vpb.AT_DARWIN_AMD64 || at == vpb.AT_LINUX_AMD64) && viper.GetString("cli_version") != "" {
		if versionStr != viper.GetString("cli_version") {
			return nil, status.Error(codes.NotFound, "artifact not found")
		}
	} else {
		query := `SELECT
					1
				FROM artifacts
				WHERE artifact_name=$1
						AND $2=ANY(available_artifacts)
						AND version_str=$3
				LIMIT 1;`

		rows, err := s.db.Query(query, name, utils.ToArtifactTypeDB(at), versionStr)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to query database")
		}
		defer rows.Close()

		if !rows.Next() {
			return nil, status.Error(codes.NotFound, "artifact not found")
		}
	}

	expires := time.Now().Add(time.Minute * 60)

	// Artifact found, generate the download link.
	// location: gs://<artifact_bucket>/cli/2019.10.03-1/cli_linux_amd64

	// If the version is not an official release, it is contained in a private bucket which requires creds to
	// generate a signed URL.
	release := !strings.Contains(versionStr, "-")
	bucket := s.artifactBucket
	if release {
		bucket = s.releaseBucket
	}
	if !release && s.gcsSA == nil {
		return nil, status.Error(codes.Internal, "Could not get download URL for non-release build without creds")
	}

	var url string
	var err error
	objectPath := path.Join(name, versionStr, fmt.Sprintf("%s_%s", name, downloadSuffix(at)))
	if !release {
		url, err = URLSigner(bucket, objectPath, &storage.SignedURLOptions{
			GoogleAccessID: s.gcsSA.Email,
			PrivateKey:     s.gcsSA.PrivateKey,
			Method:         "GET",
			Expires:        expires,
			Scheme:         0,
		})

		if err != nil {
			return nil, status.Error(codes.Internal, "failed to sign download URL")
		}
	} else {
		attr, err := s.sc.Bucket(bucket).Object(objectPath).Attrs(ctx)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to get URL")
		}
		url = attr.MediaLink
	}

	tpb, _ := types.TimestampProto(expires)

	sha256ObjectPath := objectPath + ".sha256"
	r, err := s.sc.Bucket(bucket).Object(sha256ObjectPath).NewReader(ctx)

	if err != nil {
		return nil, status.Error(codes.Internal, "failed to fetch sha256 file")
	}
	defer r.Close()

	sha256bytes, err := ioutil.ReadAll(r)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to read sha256 file")
	}

	return &apb.GetDownloadLinkResponse{
		Url:        url,
		SHA256:     strings.TrimSpace(string(sha256bytes)),
		ValidUntil: tpb,
	}, nil
}
