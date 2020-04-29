package artifacts

import (
	"context"
	"fmt"
	"io"
	"net/http"

	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/auth"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
)

func downloadFile(url string) (io.ReadCloser, error) {
	// Get the data
	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}
	return resp.Body, nil
}

func downloadVizierYAMLs(conn *grpc.ClientConn, version string) (io.ReadCloser, error) {
	client := cloudapipb.NewArtifactTrackerClient(conn)

	creds, err := auth.LoadDefaultCredentials()
	if err != nil {
		return nil, err
	}

	req := &cloudapipb.GetDownloadLinkRequest{
		ArtifactName: "vizier",
		VersionStr:   version,
		ArtifactType: cloudapipb.AT_CONTAINER_SET_YAMLS,
	}
	ctxWithCreds := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", creds.Token))

	resp, err := client.GetDownloadLink(ctxWithCreds, req)
	if err != nil {
		return nil, err
	}

	return downloadFile(resp.Url)
}

// FetchVizierYAMLMap fetches Vizier YAML files and write to a map <fname>:<yaml string>.
func FetchVizierYAMLMap(conn *grpc.ClientConn, version string) (map[string]string, error) {
	reader, err := downloadVizierYAMLs(conn, version)

	if err != nil {
		return nil, err
	}
	defer reader.Close()

	yamlMap, err := utils.ReadTarFileFromReader(reader)
	if err != nil {
		return nil, err
	}
	return yamlMap, nil
}
