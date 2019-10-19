package controller

import (
	"context"
	"errors"

	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	versionspb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/versionspb"
)

// CLIArtifactResolver is the resolver responsible for resolving the CLI artifact.
type CLIArtifactResolver struct {
	URL    string
	SHA256 string
}

type cliArtifactArgs struct {
	ArtifactType *string
}

func artifactTypeToProto(a *string) versionspb.ArtifactType {
	switch *a {
	case "AT_LINUX_AMD64":
		return versionspb.AT_LINUX_AMD64
	case "AT_DARWIN_AMD64":
		return versionspb.AT_DARWIN_AMD64
	case "AT_CONTAINER_SET_LINUX_AMD64":
		return versionspb.AT_CONTAINER_SET_LINUX_AMD64
	default:
		return versionspb.AT_UNKNOWN
	}
}

// CLIArtifact resolves CLI information.
func (q *QueryResolver) CLIArtifact(ctx context.Context, args *cliArtifactArgs) (*CLIArtifactResolver, error) {
	artifactTypePb := artifactTypeToProto(args.ArtifactType)

	artifactReq := &artifacttrackerpb.GetArtifactListRequest{
		ArtifactType: artifactTypePb,
		ArtifactName: "cli",
		Limit:        1,
	}

	cliInfo, err := q.Env.ArtifactTrackerClient().GetArtifactList(ctx, artifactReq)
	if err != nil {
		return nil, err
	}

	if len(cliInfo.Artifact) != 1 {
		return nil, errors.New("No artifact exists")
	}

	linkReq := &artifacttrackerpb.GetDownloadLinkRequest{
		ArtifactName: "cli",
		ArtifactType: artifactTypePb,
		VersionStr:   cliInfo.Artifact[0].VersionStr,
	}

	linkResp, err := q.Env.ArtifactTrackerClient().GetDownloadLink(ctx, linkReq)
	if err != nil {
		return nil, err
	}

	return &CLIArtifactResolver{
		linkResp.Url, linkResp.SHA256,
	}, nil
}
