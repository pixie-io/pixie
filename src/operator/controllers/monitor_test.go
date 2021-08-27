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
	"bytes"
	"io/ioutil"
	"net/http"
	"testing"

	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	storagev1 "k8s.io/api/storage/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes/fake"

	pixiev1alpha1 "px.dev/pixie/src/operator/api/v1alpha1"
	"px.dev/pixie/src/shared/status"
)

type FakeHTTPClient struct {
	responses map[string]string
}

func (f *FakeHTTPClient) Get(url string) (*http.Response, error) {
	if resp, ok := f.responses[url]; ok {
		status := "200"
		statusCode := 200
		if resp != "" {
			status = "503"
			statusCode = 503
		}
		return &http.Response{
			Status:     status,
			StatusCode: statusCode,
			Body:       ioutil.NopCloser(bytes.NewBufferString(resp)),
		}, nil
	}

	return &http.Response{
		Status:     "404",
		StatusCode: 404,
		Body:       ioutil.NopCloser(bytes.NewBufferString("")),
	}, nil
}

func TestMonitor_queryPodStatusz(t *testing.T) {
	httpClient := &FakeHTTPClient{
		responses: map[string]string{
			"https://127.0.0.1:8080/statusz":  "",
			"https://127.0.0.3:50100/statusz": "CloudConnectFailed",
		},
	}

	tests := []struct {
		name           string
		podPort        int32
		podIP          string
		expectedStatus string
		expectedOK     bool
	}{
		{
			name:           "OK",
			podPort:        8080,
			podIP:          "127.0.0.1",
			expectedStatus: "",
			expectedOK:     true,
		},
		{
			name:           "404",
			podPort:        50100,
			podIP:          "127.0.0.2",
			expectedStatus: "",
			expectedOK:     true,
		},
		{
			name:           "unhealthy",
			podPort:        50100,
			podIP:          "127.0.0.3",
			expectedStatus: "CloudConnectFailed",
			expectedOK:     false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ok, status := queryPodStatusz(httpClient, &v1.Pod{
				Status: v1.PodStatus{
					PodIP: test.podIP,
				},
				Spec: v1.PodSpec{
					Containers: []v1.Container{
						v1.Container{
							Ports: []v1.ContainerPort{
								v1.ContainerPort{
									ContainerPort: test.podPort,
								},
							},
						},
					},
				},
			})

			assert.Equal(t, test.expectedStatus, status)
			assert.Equal(t, test.expectedOK, ok)
		})
	}
}

func TestMonitor_getCloudConnState(t *testing.T) {
	tests := []struct {
		name                string
		cloudConnStatusz    string
		cloudConnPhase      v1.PodPhase
		expectedVizierPhase pixiev1alpha1.VizierPhase
		expectedReason      status.VizierReason
	}{
		{
			name:                "healthy",
			cloudConnStatusz:    "",
			cloudConnPhase:      v1.PodRunning,
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			expectedReason:      "",
		},
		{
			name:                "updating",
			cloudConnStatusz:    "",
			cloudConnPhase:      v1.PodPending,
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUpdating,
			expectedReason:      status.CloudConnectorPodPending,
		},
		{
			name:                "unhealthy but running",
			cloudConnStatusz:    "CloudConnectFailed",
			cloudConnPhase:      v1.PodRunning,
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			expectedReason:      status.CloudConnectorFailedToConnect,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			httpClient := &FakeHTTPClient{
				responses: map[string]string{
					"https://127.0.0.1:8080/statusz": test.cloudConnStatusz,
				},
			}

			pods := &concurrentPodMap{
				unsafeMap: map[string]*v1.Pod{
					"vizier-cloud-connector": &v1.Pod{
						Status: v1.PodStatus{
							PodIP: "127.0.0.1",
							Phase: test.cloudConnPhase,
						},
						Spec: v1.PodSpec{
							Containers: []v1.Container{
								v1.Container{
									Ports: []v1.ContainerPort{
										v1.ContainerPort{
											ContainerPort: 8080,
										},
									},
								},
							},
						},
					},
				},
			}

			state := getCloudConnState(httpClient, pods)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}

func TestMonitor_getPVCState(t *testing.T) {
	tests := []struct {
		name                     string
		pvcStorageClassName      string
		clusterStorageClassNames []string
		pvcPhase                 v1.PersistentVolumeClaimPhase
		expectedVizierPhase      pixiev1alpha1.VizierPhase
		expectedReason           status.VizierReason
	}{
		{
			name:                     "healthy",
			pvcPhase:                 v1.ClaimBound,
			pvcStorageClassName:      "standard",
			clusterStorageClassNames: []string{"standard"},
			expectedVizierPhase:      pixiev1alpha1.VizierPhaseHealthy,
			expectedReason:           "",
		},
		{
			name:                     "true_claim_pending",
			pvcPhase:                 v1.ClaimPending,
			pvcStorageClassName:      "standard",
			clusterStorageClassNames: []string{"standard"},
			expectedVizierPhase:      pixiev1alpha1.VizierPhaseUpdating,
			expectedReason:           status.MetadataPVCPendingBinding,
		},
		{
			name:                     "storage class not available on cluster",
			pvcPhase:                 v1.ClaimPending,
			pvcStorageClassName:      "standard",
			clusterStorageClassNames: []string{},
			expectedVizierPhase:      pixiev1alpha1.VizierPhaseUnhealthy,
			expectedReason:           status.MetadataPVCStorageClassUnavailable,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pvc := &concurrentPVCMap{
				unsafeMap: map[string]*v1.PersistentVolumeClaim{
					"metadata-pv-claim": &v1.PersistentVolumeClaim{
						Status: v1.PersistentVolumeClaimStatus{
							Phase: test.pvcPhase,
						},
						Spec: v1.PersistentVolumeClaimSpec{
							StorageClassName: &test.pvcStorageClassName,
						},
					},
				},
			}

			list := &storagev1.StorageClassList{Items: make([]storagev1.StorageClass, 0)}
			for _, className := range test.clusterStorageClassNames {
				list.Items = append(list.Items, storagev1.StorageClass{
					ObjectMeta: metav1.ObjectMeta{Name: className},
				})
			}
			clientset := fake.NewSimpleClientset(list)

			state := getMetadataPVCState(clientset, pvc)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}
