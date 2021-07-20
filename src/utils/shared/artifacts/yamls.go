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

package artifacts

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"regexp"
	"sort"

	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/utils/shared/tar"
	"px.dev/pixie/src/utils/shared/yamls"
)

func downloadFile(url string) (io.ReadCloser, error) {
	// Get the data
	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}
	return resp.Body, nil
}

func downloadVizierYAMLs(conn *grpc.ClientConn, authToken, versionStr string, templated bool) (io.ReadCloser, error) {
	client := cloudpb.NewArtifactTrackerClient(conn)
	at := cloudpb.AT_CONTAINER_SET_YAMLS
	if templated {
		at = cloudpb.AT_CONTAINER_SET_TEMPLATE_YAMLS
	}

	req := &cloudpb.GetDownloadLinkRequest{
		ArtifactName: "vizier",
		VersionStr:   versionStr,
		ArtifactType: at,
	}
	ctxWithCreds := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", authToken))

	resp, err := client.GetDownloadLink(ctxWithCreds, req)
	if err != nil {
		return nil, err
	}

	return downloadFile(resp.Url)
}

// FetchVizierYAMLMap fetches Vizier YAML files and write to a map <fname>:<yaml string>.
func FetchVizierYAMLMap(conn *grpc.ClientConn, authToken, versionStr string) (map[string]string, error) {
	reader, err := downloadVizierYAMLs(conn, authToken, versionStr, false)

	if err != nil {
		return nil, err
	}
	defer reader.Close()

	yamlMap, err := tar.ReadTarFileFromReader(reader)
	if err != nil {
		return nil, err
	}
	return yamlMap, nil
}

// FetchVizierTemplates fetches the Vizier templates for the given version.
func FetchVizierTemplates(conn *grpc.ClientConn, authToken, versionStr string) ([]*yamls.YAMLFile, error) {
	reader, err := downloadVizierYAMLs(conn, authToken, versionStr, true)

	if err != nil {
		return nil, err
	}
	defer reader.Close()

	yamlMap, err := tar.ReadTarFileFromReader(reader)
	if err != nil {
		return nil, err
	}

	// Convert to YAML files, using the provided file names.
	// Get the YAML names, in order.
	yamlNames := make([]string, len(yamlMap))
	i := 0
	for k := range yamlMap {
		yamlNames[i] = k
		i++
	}
	sort.Strings(yamlNames)

	// Write to YAMLFile slice.
	var yamlFiles []*yamls.YAMLFile
	re := regexp.MustCompile(`(?:[0-9]+_)(.*)(?:\.yaml)`)
	for _, fName := range yamlNames {
		// The filename looks like "./pixie_yamls/00_namespace.yaml", we want to extract the "namespace".
		ms := re.FindStringSubmatch(fName)
		if ms == nil || len(ms) != 2 {
			continue
		}
		yamlFiles = append(yamlFiles, &yamls.YAMLFile{
			Name: ms[1],
			YAML: yamlMap[fName],
		})
	}

	return yamlFiles, nil
}

// FetchOperatorTemplates fetches the operator templates for the given version.
func FetchOperatorTemplates(conn *grpc.ClientConn, versionStr string) ([]*yamls.YAMLFile, error) {
	client := cloudpb.NewArtifactTrackerClient(conn)

	req := &cloudpb.GetDownloadLinkRequest{
		ArtifactName: "operator",
		VersionStr:   versionStr,
		ArtifactType: cloudpb.AT_CONTAINER_SET_TEMPLATE_YAMLS,
	}

	resp, err := client.GetDownloadLink(context.Background(), req)
	if err != nil {
		return nil, err
	}

	reader, err := downloadFile(resp.Url)
	if err != nil {
		return nil, err
	}
	defer reader.Close()

	yamlMap, err := tar.ReadTarFileFromReader(reader)
	if err != nil {
		return nil, err
	}

	// Convert to YAML files, using the provided file names.
	// Get the YAML names, in order.
	yamlNames := make([]string, len(yamlMap))
	i := 0
	for k := range yamlMap {
		yamlNames[i] = k
		i++
	}
	sort.Strings(yamlNames)

	// Write to YAMLFile slice.
	var yamlFiles []*yamls.YAMLFile
	re := regexp.MustCompile(`((?:[0-9]+_)|(?:\/crds\/))(.*)(?:\.yaml)`)
	for _, fName := range yamlNames {
		// The filename looks like "./pixie_yamls/00_namespace.yaml" or "./pixie_yamls/crds/vizier_crd.yaml", we want to extract the "namespace".
		ms := re.FindStringSubmatch(fName)
		if ms == nil || len(ms) != 3 {
			continue
		}
		yamlFiles = append(yamlFiles, &yamls.YAMLFile{
			Name: ms[2],
			YAML: yamlMap[fName],
		})
	}

	return yamlFiles, nil
}
