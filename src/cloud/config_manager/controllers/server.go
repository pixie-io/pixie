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
	"net/http"
	"regexp"
	"sort"
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"k8s.io/apimachinery/pkg/api/meta"
	"k8s.io/client-go/kubernetes"

	atpb "px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	cpb "px.dev/pixie/src/cloud/config_manager/configmanagerpb"
	versionspb "px.dev/pixie/src/shared/artifacts/versionspb"
	"px.dev/pixie/src/utils/shared/tar"
	yamls "px.dev/pixie/src/utils/shared/yamls"
	vizieryamls "px.dev/pixie/src/utils/template_generator/vizier_yamls"
)

// Server defines an gRPC server type.
type Server struct {
	atClient  atpb.ArtifactTrackerClient
	clientset *kubernetes.Clientset
	rm        meta.RESTMapper
}

// NewServer creates GRPC handlers.
func NewServer(atClient atpb.ArtifactTrackerClient, clientset *kubernetes.Clientset, rm meta.RESTMapper) *Server {
	return &Server{
		atClient:  atClient,
		clientset: clientset,
		rm:        rm,
	}
}

func downloadFile(url string) (io.ReadCloser, error) {
	// Get the data
	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}
	return resp.Body, nil
}

// GetConfigForVizier provides yaml names and content that can be used to deploy Vizier
func (s *Server) GetConfigForVizier(ctx context.Context,
	in *cpb.ConfigForVizierRequest) (*cpb.ConfigForVizierResponse, error) {
	templatedYAMLs, err := fetchVizierTemplates(ctx, "", in.VzSpec.Version, s.atClient)
	if err != nil {
		log.WithError(err).Error("Failed to fetch Vizier templates")
		return nil, err
	}

	// Fill in template values.
	cloudAddr := in.VzSpec.CloudAddr
	updateCloudAddr := in.VzSpec.CloudAddr
	if in.VzSpec.DevCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", in.VzSpec.DevCloudNamespace)
		updateCloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", in.VzSpec.DevCloudNamespace)
	}

	// We should eventually clean up the templating code, since our Helm charts and extracted YAMLs will now just
	// be simple CRDs.
	tmplValues := &vizieryamls.VizierTmplValues{
		DeployKey:         in.VzSpec.DeployKey,
		UseEtcdOperator:   in.VzSpec.UseEtcdOperator,
		PEMMemoryLimit:    in.VzSpec.PemMemoryLimit,
		Namespace:         in.Namespace,
		CloudAddr:         cloudAddr,
		CloudUpdateAddr:   updateCloudAddr,
		ClusterName:       in.VzSpec.ClusterName,
		DisableAutoUpdate: in.VzSpec.DisableAutoUpdate,
		SentryDSN:         getSentryDSN(in.VzSpec.Version),
		ClockConverter:    in.VzSpec.ClockConverter,
		DataAccess:        in.VzSpec.DataAccess,
	}

	if in.VzSpec.DataCollectorParams != nil && in.VzSpec.DataCollectorParams.DatastreamBufferSize != 0 {
		tmplValues.DatastreamBufferSize = in.VzSpec.DataCollectorParams.DatastreamBufferSize
	}
	if in.VzSpec.DataCollectorParams != nil && in.VzSpec.DataCollectorParams.DatastreamBufferSpikeSize != 0 {
		tmplValues.DatastreamBufferSpikeSize = in.VzSpec.DataCollectorParams.DatastreamBufferSpikeSize
	}
	if in.VzSpec.DataCollectorParams != nil && in.VzSpec.DataCollectorParams.TableStoreTableSizeLimit != 0 {
		tmplValues.TableStoreTableSizeLimit = in.VzSpec.DataCollectorParams.TableStoreTableSizeLimit
	}
	if in.VzSpec.DataCollectorParams != nil && in.VzSpec.DataCollectorParams.CustomPEMFlags != nil {
		tmplValues.CustomPEMFlags = in.VzSpec.DataCollectorParams.CustomPEMFlags
	}

	if in.VzSpec.LeadershipElectionParams != nil {
		tmplValues.ElectionPeriodMs = in.VzSpec.LeadershipElectionParams.ElectionPeriodMs
	}

	vzYamls, err := yamls.ExecuteTemplatedYAMLs(templatedYAMLs, vizieryamls.VizierTmplValuesToArgs(tmplValues))
	if err != nil {
		log.WithError(err).Error("Failed to execute templates")
		return nil, err
	}

	// Apply custom patches, if any.
	if in.VzSpec.Patches != nil || len(in.VzSpec.Patches) > 0 {
		for _, y := range vzYamls {
			patchedYAML, err := yamls.AddPatchesToYAML(s.clientset, y.YAML, in.VzSpec.Patches, s.rm)
			if err != nil {
				log.WithError(err).Error("Failed to add patches")
				return nil, err
			}
			y.YAML = patchedYAML
		}
	}

	// Map from the YAML name to the YAML contents.
	yamlMap := make(map[string]string)
	for _, y := range vzYamls {
		yamlMap[y.Name] = y.YAML
	}

	return &cpb.ConfigForVizierResponse{
		NameToYamlContent: yamlMap,
		SentryDSN:         getSentryDSN(in.VzSpec.Version),
	}, nil
}

func getSentryDSN(vizierVersion string) string {
	// If it contains - it must be a pre-release Vizier.
	if strings.Contains(vizierVersion, "-") {
		return viper.GetString("dev_sentry")
	}
	return viper.GetString("prod_sentry")
}

// fetchVizierTemplates gets a download link, untars file, and
// converts to yaml maps.
func fetchVizierTemplates(ctx context.Context, authToken,
	versionStr string, atClient atpb.ArtifactTrackerClient) ([]*yamls.YAMLFile, error) {
	req := &atpb.GetDownloadLinkRequest{
		ArtifactName: "vizier",
		VersionStr:   versionStr,
		ArtifactType: versionspb.AT_CONTAINER_SET_TEMPLATE_YAMLS,
	}
	resp, err := atClient.GetDownloadLink(ctx, req)
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
