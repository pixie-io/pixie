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

package controllers_test

import (
	"context"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vizierconfigpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/config_manager/configmanagerpb"
)

func TestConfigForVizier(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			vzSpec := &vizierconfigpb.VizierSpec{
				Version:           "",
				DeployKey:         "",
				DisableAutoUpdate: false,
				UseEtcdOperator:   false,
				ClusterName:       "test-cluster",
				CloudAddr:         "",
				DevCloudNamespace: "plc-dev",
				PemMemoryLimit:    "10Mi",
				PemMemoryRequest:  "6Mi",
				Pod_Policy:        nil,
				Patches: map[string]string{
					"vizier-pem": `{ "spec": { "template": {"spec": { "tolerations": [{"key": "test", "operator": "Equals", "effect": "NoExecute" }]} }}  }`,
				},
			}

			mockReq := &configmanagerpb.ConfigForVizierRequest{
				Namespace:  "test-namespace",
				VzSpec:     vzSpec,
				K8sVersion: "1.24.1",
			}

			nameToYamlContent := make(map[string]string)
			nameToYamlContent["fileAName"] = "fileAContent"
			mockClients.MockConfigMgr.EXPECT().GetConfigForVizier(gomock.Any(), mockReq).
				Return(&configmanagerpb.ConfigForVizierResponse{
					NameToYamlContent: nameToYamlContent,
				}, nil)

			cfgServer := &controllers.ConfigServiceServer{mockClients.MockConfigMgr}

			resp, err := cfgServer.GetConfigForVizier(ctx, &cloudpb.ConfigForVizierRequest{
				Namespace:  "test-namespace",
				VzSpec:     vzSpec,
				K8sVersion: "1.24.1",
			})
			require.NoError(t, err)
			assert.Equal(t, resp.NameToYamlContent["fileAName"], "fileAContent")
		})
	}
}
