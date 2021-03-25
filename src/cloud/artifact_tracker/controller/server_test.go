package controller_test

import (
	"context"
	"fmt"
	"os"
	"testing"
	"time"

	"cloud.google.com/go/storage"
	"github.com/gogo/protobuf/types"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	"github.com/jmoiron/sqlx"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/oauth2/jwt"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	apb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/controller"
	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/schema"
	vpb "pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestMain(m *testing.M) {
	err := testMain(m)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Got error: %v\n", err)
		os.Exit(1)
	}
	os.Exit(0)
}

var db *sqlx.DB

func testMain(m *testing.M) error {
	s := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})

	testDB, teardown, err := pgtest.SetupTestDB(s)
	if err != nil {
		return fmt.Errorf("failed to start test database: %w", err)
	}

	defer teardown()
	db = testDB

	if c := m.Run(); c != 0 {
		return fmt.Errorf("some tests failed with code: %d", c)
	}
	return nil
}

func mustSetupFakeBucket(t *testing.T) stiface.Client {
	return testingutils.NewMockGCSClient(map[string]*testingutils.MockGCSBucket{
		"test-bucket": testingutils.NewMockGCSBucket(
			map[string]*testingutils.MockGCSObject{
				"cli/1.2.1-pre.3/cli_linux_amd64.sha256": testingutils.NewMockGCSObject([]byte("the-sha256"), nil),
			},
			nil,
		),
	})
}

func mustLoadTestData(db *sqlx.DB) {
	db.MustExec(`DELETE from artifact_changelogs`)
	db.MustExec(`DELETE FROM artifacts`)

	insertArtifactQuery := `
        INSERT INTO artifacts
          (id, artifact_name, create_time, commit_hash, version_str, available_artifacts)
        VALUES
          ($1, $2, $3, $4, $5, $6)`
	db.MustExec(insertArtifactQuery, "123e4567-e89b-12d3-a456-426655440000",
		"cli", "2019-06-22 19:10:25-07", "bda4ac2f4c979e81f5d95a2b550a08fb041e985c",
		"1.2.3", "{LINUX_AMD64, DARWIN_AMD64}")
	db.MustExec(insertArtifactQuery, "123e4567-e89b-12d3-a456-426655440001",
		"cli", "2019-06-22 18:10:25-07", "ada4ac2f4c979e81f5d95a2b550a08fb041e985c",
		"1.2.1-pre.3", "{LINUX_AMD64}")
	db.MustExec(insertArtifactQuery, "123e4567-e89b-12d3-a456-426655440002",
		"cli", "2019-06-21 19:10:25-07", "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
		"1.1.5", "{LINUX_AMD64, DARWIN_AMD64}")

	db.MustExec(insertArtifactQuery, "223e4567-e89b-12d3-a456-426655440000",
		"vizier", "2019-06-21 19:10:25-07", "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
		"1.2.0", "{CONTAINER_SET_LINUX_AMD64}")

	db.MustExec(insertArtifactQuery, "223e4567-e89b-12d3-a456-426655440001",
		"vizier", "2019-06-21 17:10:25-07", "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
		"1.1.5", "{CONTAINER_SET_LINUX_AMD64}")

	insertChangelogQuery := `
        INSERT INTO artifact_changelogs
          (artifacts_id, changelog)
        VALUES
          ($1, $2)`

	db.MustExec(insertChangelogQuery, "123e4567-e89b-12d3-a456-426655440000", "cl 0")
	db.MustExec(insertChangelogQuery, "123e4567-e89b-12d3-a456-426655440001", "cl 1")
	db.MustExec(insertChangelogQuery, "123e4567-e89b-12d3-a456-426655440002", "cl 2")
	db.MustExec(insertChangelogQuery, "223e4567-e89b-12d3-a456-426655440000", "cl2 0")
	db.MustExec(insertChangelogQuery, "223e4567-e89b-12d3-a456-426655440001", "cl2 1")
}

func TestServer_GetArtifactList(t *testing.T) {
	mustLoadTestData(db)

	server := controller.NewServer(db, nil, "bucket", nil)

	testCases := []struct {
		name         string
		req          apb.GetArtifactListRequest
		expectedResp *vpb.ArtifactSet
		err          error
	}{
		{
			name: "cli linux limit 1 should return 1 linux artifact",
			req: apb.GetArtifactListRequest{
				ArtifactName: "cli",
				ArtifactType: vpb.AT_LINUX_AMD64,
				Limit:        1,
			},
			expectedResp: &vpb.ArtifactSet{
				Name: "cli",
				Artifact: []*vpb.Artifact{
					{
						Timestamp:          &types.Timestamp{Seconds: 1561230625},
						CommitHash:         "bda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.2.3",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 0",
					},
				},
			},
			err: nil,
		},
		{
			name: "cli linux limit 0 should return 2 linux artifacts",
			req: apb.GetArtifactListRequest{
				ArtifactName: "cli",
				ArtifactType: vpb.AT_LINUX_AMD64,
				Limit:        0,
			},
			expectedResp: &vpb.ArtifactSet{
				Name: "cli",
				Artifact: []*vpb.Artifact{
					{
						Timestamp:          &types.Timestamp{Seconds: 1561230625},
						CommitHash:         "bda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.2.3",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 0",
					},
					{
						Timestamp:          &types.Timestamp{Seconds: 1561144225},
						CommitHash:         "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.1.5",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 2",
					},
				},
			},
			err: nil,
		},
		{
			name: "cli linux limit 2 should return 2 linux artifacts",
			req: apb.GetArtifactListRequest{
				ArtifactName: "cli",
				ArtifactType: vpb.AT_LINUX_AMD64,
				Limit:        2,
			},
			expectedResp: &vpb.ArtifactSet{
				Name: "cli",
				Artifact: []*vpb.Artifact{
					{
						Timestamp:          &types.Timestamp{Seconds: 1561230625},
						CommitHash:         "bda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.2.3",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 0",
					},
					{
						Timestamp:          &types.Timestamp{Seconds: 1561144225},
						CommitHash:         "cda4ac2f4c979e81f5d95a2b550a08fb041e985c",
						VersionStr:         "1.1.5",
						AvailableArtifacts: []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64},
						Changelog:          "cl 2",
					},
				},
			},
			err: nil,
		},
		{
			name: "vizier limit 1 should return empty set",
			req: apb.GetArtifactListRequest{
				ArtifactName: "vizier",
				ArtifactType: vpb.AT_LINUX_AMD64,
				Limit:        1,
			},
			expectedResp: &vpb.ArtifactSet{
				Name:     "vizier",
				Artifact: []*vpb.Artifact{},
			},
			err: nil,
		},
		{
			name: "missing artifact type is an error",
			req: apb.GetArtifactListRequest{
				ArtifactName: "vizier",
				Limit:        1,
			},
			expectedResp: nil,
			err:          status.Error(codes.InvalidArgument, "missing artifact type"),
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			resp, err := server.GetArtifactList(context.Background(), &tc.req)
			if tc.err != nil {
				assert.Equal(t, status.Code(err), status.Code(tc.err))
			} else {
				require.NoError(t, err)
				assert.Equal(t, resp, tc.expectedResp)
			}
		})
	}
}

func TestServer_GetDownloadLink(t *testing.T) {
	mustLoadTestData(db)
	storageClient := mustSetupFakeBucket(t)

	server := controller.NewServer(db, storageClient, "test-bucket", &jwt.Config{
		Email:      "test@test.com",
		PrivateKey: []byte("the-key"),
	})

	testCases := []struct {
		name         string
		req          apb.GetDownloadLinkRequest
		expectedResp *apb.GetDownloadLinkResponse
		errCode      codes.Code
	}{
		{
			name: "missing artifact name should give an error",
			req: apb.GetDownloadLinkRequest{
				VersionStr:   "2019.21.1",
				ArtifactType: vpb.AT_LINUX_AMD64,
			},
			errCode: codes.InvalidArgument,
		},
		{
			name: "missing version string should give an error",
			req: apb.GetDownloadLinkRequest{
				ArtifactName: "cli",
				ArtifactType: vpb.AT_LINUX_AMD64,
			},
			errCode: codes.InvalidArgument,
		},
		{
			name: "missing artifact type should give an error",
			req: apb.GetDownloadLinkRequest{
				ArtifactName: "cli",
				VersionStr:   "2019.21.1",
			},
			errCode: codes.InvalidArgument,
		},
		{
			name: "not downloadable artifact should give an error",
			req: apb.GetDownloadLinkRequest{
				ArtifactName: "cli",
				VersionStr:   "2019.21.1",
				ArtifactType: vpb.AT_CONTAINER_SET_LINUX_AMD64,
			},
			errCode: codes.InvalidArgument,
		},
		{
			name: "Linux CLI fetch",
			req: apb.GetDownloadLinkRequest{
				ArtifactName: "cli",
				VersionStr:   "1.2.1-pre.3",
				ArtifactType: vpb.AT_LINUX_AMD64,
			},
			expectedResp: &apb.GetDownloadLinkResponse{
				Url:    "the-url",
				SHA256: "the-sha256",
			},
			errCode: codes.OK,
		},
	}

	controller.URLSigner = func(bucket, name string, opts *storage.SignedURLOptions) (s string, err error) {
		return "the-url", nil
	}
	// Only testing error cases for now because the storage API is hard to mock.
	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			resp, err := server.GetDownloadLink(context.Background(), &tc.req)
			if tc.errCode != codes.OK {
				assert.Equal(t, status.Code(err), tc.errCode)
				assert.Nil(t, resp)
			} else {
				require.NoError(t, err)
				assert.Equal(t, resp.Url, "the-url")

				ts, err := types.TimestampFromProto(resp.ValidUntil)
				require.NoError(t, err)
				assert.True(t, time.Until(ts) > 0)
				assert.Equal(t, resp.Url, tc.expectedResp.Url)
				assert.Equal(t, resp.SHA256, tc.expectedResp.SHA256)
			}
		})
	}
}
