package controller

import (
	"context"
	"database/sql/driver"
	"errors"
	"fmt"
	"io/ioutil"
	"path"
	"strings"
	"time"

	"github.com/googleapis/google-cloud-go-testing/storage/stiface"

	"golang.org/x/oauth2/jwt"

	"cloud.google.com/go/storage"

	"github.com/gogo/protobuf/types"
	"github.com/jmoiron/sqlx"
	"github.com/lib/pq"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	apb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	vpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/versionspb"
)

// artifactType values
const (
	atUnknown                artifactType = "UNKNOWN"
	atLinuxAMD64             artifactType = "LINUX_AMD64"
	atDarwinAMD64            artifactType = "DARWIN_AMD64"
	atContainerSetLinuxAMD64 artifactType = "CONTAINER_SET_LINUX_AMD64"
)

// URLSigner is the function used to sign urls.
var URLSigner = storage.SignedURL

type artifactType string

func (s *artifactType) Scan(value interface{}) error {
	asBytes, ok := value.([]byte)
	if !ok {
		return errors.New("Scan source is not []byte")
	}
	*s = artifactType(string(asBytes))
	return nil
}

func (s artifactType) Value() (driver.Value, error) {
	fmt.Print(s)
	return string(s), nil
}

// Server is the controller for the atrifact tracker service.
type Server struct {
	db             *sqlx.DB
	sc             stiface.Client
	artifactBucket string
	gcsSA          *jwt.Config
}

// NewServer creates a new artifact tracker server.
func NewServer(db *sqlx.DB, client stiface.Client, bucket string, gcsSA *jwt.Config) *Server {
	return &Server{db: db, sc: client, artifactBucket: bucket, gcsSA: gcsSA}
}

func toArtifactType(a vpb.ArtifactType) artifactType {
	switch a {
	case vpb.AT_LINUX_AMD64:
		return atLinuxAMD64
	case vpb.AT_DARWIN_AMD64:
		return atDarwinAMD64
	case vpb.AT_CONTAINER_SET_LINUX_AMD64:
		return atContainerSetLinuxAMD64
	default:
		return atUnknown
	}
}

func toProtoArtifactType(a artifactType) vpb.ArtifactType {
	switch a {
	case atLinuxAMD64:
		return vpb.AT_LINUX_AMD64
	case atDarwinAMD64:
		return vpb.AT_DARWIN_AMD64
	case atContainerSetLinuxAMD64:
		return vpb.AT_CONTAINER_SET_LINUX_AMD64
	default:
		return vpb.AT_UNKNOWN
	}
}

func toProtoArtifactTypeArray(a pq.StringArray) []vpb.ArtifactType {
	res := make([]vpb.ArtifactType, len(a))
	for i, val := range a {
		res[i] = toProtoArtifactType(artifactType(val))
	}
	return res
}

// GetArtifactList returns a list of artifacts matching the passed in criteria.
func (s *Server) GetArtifactList(ctx context.Context, in *apb.GetArtifactListRequest) (*vpb.ArtifactSet, error) {
	name := in.ArtifactName
	at := toArtifactType(in.ArtifactType)
	limit := in.Limit

	if at == atUnknown {
		return nil, status.Error(codes.InvalidArgument, "artifact type cannot be unknown")
	}

	// Limit was not specified, show the latest.
	if limit == 0 {
		limit = 1
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
			AvailableArtifacts: toProtoArtifactTypeArray(res.AvailableArtifacts),
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
              ORDER BY create_time DESC LIMIT $3;`

	rows, err := s.db.Queryx(query, name, at, limit)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to query database")
	}

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

	if !(at == vpb.AT_DARWIN_AMD64 || at == vpb.AT_LINUX_AMD64) {
		return nil, status.Error(codes.InvalidArgument, "artifact type cannot be downloaded")
	}

	query := `SELECT 
                1
              FROM artifacts 
              WHERE artifact_name=$1 
                    AND $2=ANY(available_artifacts)
                    AND version_str=$3
	          LIMIT 1;`

	rows, err := s.db.Query(query, name, toArtifactType(at), versionStr)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to query database")
	}
	defer rows.Close()

	if !rows.Next() {
		return nil, status.Error(codes.NotFound, "artifact not found")
	}

	expires := time.Now().Add(time.Minute * 60)

	// Artifact found, generate the download link.
	// location: gs://<artifact_bucket>/cli/2019.10.03-1/cli_linux_amd64
	objectPath := path.Join(name, versionStr, fmt.Sprintf("%s_%s", name, downloadSuffix(at)))
	url, err := URLSigner(s.artifactBucket, objectPath, &storage.SignedURLOptions{
		GoogleAccessID: s.gcsSA.Email,
		PrivateKey:     s.gcsSA.PrivateKey,
		Method:         "GET",
		Expires:        expires,
		Scheme:         0,
	})

	if err != nil {
		return nil, status.Error(codes.Internal, "failed to sign download URL")
	}
	tpb, _ := types.TimestampProto(expires)

	sha256ObjectPath := objectPath + ".sha256"
	r, err := s.sc.Bucket(s.artifactBucket).Object(sha256ObjectPath).NewReader(ctx)
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
