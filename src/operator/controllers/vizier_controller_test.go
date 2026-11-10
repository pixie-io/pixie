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
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/kubernetes/fake"

	"px.dev/pixie/src/api/proto/cloudpb"
	mock_cloudpb "px.dev/pixie/src/api/proto/cloudpb/mock"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/utils"
)

const testVizierID = "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

func TestGetVizierID(t *testing.T) {
	tests := []struct {
		name       string
		secretData map[string][]byte
		expectedID *uuidpb.UUID
	}{
		{
			name:       "missing secret",
			secretData: nil,
			expectedID: nil,
		},
		{
			name:       "missing cluster ID",
			secretData: map[string][]byte{},
			expectedID: nil,
		},
		{
			name: "invalid cluster ID",
			secretData: map[string][]byte{
				"cluster-id": []byte("not-a-uuid"),
			},
			expectedID: nil,
		},
		{
			name: "valid cluster ID",
			secretData: map[string][]byte{
				"cluster-id": []byte(testVizierID),
			},
			expectedID: utils.ProtoFromUUIDStrOrNil(testVizierID),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			var objects []runtime.Object
			if test.secretData != nil {
				objects = append(objects, &v1.Secret{
					ObjectMeta: metav1.ObjectMeta{
						Name:      "pl-cluster-secrets",
						Namespace: "pl",
					},
					Data: test.secretData,
				})
			}
			clientset := fake.NewSimpleClientset(objects...)

			assert.Equal(t, test.expectedID, getVizierID(clientset, "pl"))
		})
	}
}

func TestGenerateVizierYAMLsConfig_VizierID(t *testing.T) {
	tests := []struct {
		name     string
		vizierID *uuidpb.UUID
	}{
		{
			name:     "initial deploy",
			vizierID: nil,
		},
		{
			name:     "update",
			vizierID: utils.ProtoFromUUIDStrOrNil(testVizierID),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			client := mock_cloudpb.NewMockConfigServiceClient(ctrl)

			resp := &cloudpb.ConfigForVizierResponse{
				NameToYamlContent: map[string]string{"vizier": "yaml"},
			}
			client.EXPECT().GetConfigForVizier(gomock.Any(), gomock.Any()).
				DoAndReturn(func(_ context.Context, req *cloudpb.ConfigForVizierRequest, _ ...grpc.CallOption) (*cloudpb.ConfigForVizierResponse, error) {
					assert.Equal(t, "pl", req.Namespace)
					assert.Equal(t, "1.30.0", req.K8sVersion)
					assert.Equal(t, test.vizierID, req.VizierID)
					assert.Equal(t, "deploy-key", req.VzSpec.DeployKey)
					return resp, nil
				})

			vz := &v1alpha1.Vizier{
				Spec: v1alpha1.VizierSpec{
					DeployKey: "deploy-key",
					Pod:       &v1alpha1.PodPolicy{},
				},
			}
			actualResp, err := generateVizierYAMLsConfig(context.Background(), "pl", "1.30.0", test.vizierID, vz, client)
			assert.NoError(t, err)
			assert.Equal(t, resp, actualResp)
		})
	}
}
