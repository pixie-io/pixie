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

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vizierconfigpb"
	"px.dev/pixie/src/cloud/config_manager/configmanagerpb"
)

// ConfigServiceServer sets vizier related configurations.
type ConfigServiceServer struct {
	ConfigServiceClient configmanagerpb.ConfigManagerServiceClient
}

// GetConfigForVizier fetches vizier templates and sets up yaml maps by calling
// Config Manager service.
func (c *ConfigServiceServer) GetConfigForVizier(ctx context.Context,
	req *cloudpb.ConfigForVizierRequest) (*cloudpb.ConfigForVizierResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	vizSpecReq := req.VzSpec
	resp, err := c.ConfigServiceClient.GetConfigForVizier(ctx, &configmanagerpb.ConfigForVizierRequest{
		Namespace: req.Namespace,
		VzSpec: &vizierconfigpb.VizierSpec{
			Version:           vizSpecReq.Version,
			DeployKey:         vizSpecReq.DeployKey,
			DisableAutoUpdate: vizSpecReq.DisableAutoUpdate,
			UseEtcdOperator:   vizSpecReq.UseEtcdOperator,
			ClusterName:       vizSpecReq.ClusterName,
			CloudAddr:         vizSpecReq.CloudAddr,
			DevCloudNamespace: vizSpecReq.DevCloudNamespace,
			PemMemoryLimit:    vizSpecReq.PemMemoryLimit,
			Pod_Policy:        vizSpecReq.Pod_Policy,
		},
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.ConfigForVizierResponse{
		NameToYamlContent: resp.NameToYamlContent,
	}, nil
}
