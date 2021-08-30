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

			pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWithEvents)}
			pods.write(
				"vizier-cloud-connector",
				"vizier-cloud-connector-abcdefg",
				&podWithEvents{
					pod: &v1.Pod{
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
				})

			state := getCloudConnState(httpClient, pods)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}

func TestMonitor_getCloudConnState_SeveralCloudConns(t *testing.T) {
	httpClient := &FakeHTTPClient{
		responses: map[string]string{
			"https://127.0.0.1:8080/statusz": "",
		},
	}

	pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWithEvents)}

	spec := v1.PodSpec{
		Containers: []v1.Container{
			v1.Container{
				Ports: []v1.ContainerPort{
					v1.ContainerPort{
						ContainerPort: 8080,
					},
				},
			},
		},
	}
	pods.write("vizier-cloud-connector", "vizier-cloud-connector-abcdefg", &podWithEvents{
		pod: &v1.Pod{
			Status: v1.PodStatus{
				PodIP: "127.0.0.1",
				Phase: v1.PodRunning,
			},
			Spec: spec,
		},
	})
	pods.write("vizier-cloud-connector", "vizier-cloud-connector-12345678", &podWithEvents{
		pod: &v1.Pod{
			Status: v1.PodStatus{
				PodIP: "127.0.0.1",
				Phase: v1.PodPending,
			},
			Spec: spec,
		},
	})

	state := getCloudConnState(httpClient, pods)
	assert.Equal(t, status.CloudConnectorPodPending, state.Reason)
	assert.Equal(t, pixiev1alpha1.VizierPhaseUpdating, translateReasonToPhase(state.Reason))
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

func TestMonitor_natsPod(t *testing.T) {
	httpClient := &FakeHTTPClient{
		responses: map[string]string{
			"http://127.0.0.1:8222/": "",
			"http://127.0.0.3:8222/": "NATS Failed",
		},
	}

	tests := []struct {
		name                string
		podMissing          bool
		natsIP              string
		natsPhase           v1.PodPhase
		expectedReason      status.VizierReason
		expectedVizierPhase pixiev1alpha1.VizierPhase
	}{
		{
			name:                "OK",
			natsIP:              "127.0.0.1",
			natsPhase:           v1.PodRunning,
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "pending",
			natsIP:              "127.0.0.2",
			natsPhase:           v1.PodPending,
			expectedReason:      "NATSPodPending",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUpdating,
		},
		{
			name:                "missing",
			podMissing:          true,
			expectedReason:      "NATSPodMissing",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
		},
		{
			name:                "unhealthy",
			natsIP:              "127.0.0.3",
			natsPhase:           v1.PodRunning,
			expectedReason:      "NATSPodFailed",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWithEvents)}
			if !test.podMissing {
				pods.write(
					"",
					natsName,
					&podWithEvents{
						pod: &v1.Pod{
							Status: v1.PodStatus{
								PodIP: test.natsIP,
								Phase: test.natsPhase,
							},
						},
					},
				)
			}

			state := getNATSState(httpClient, pods)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}

type phasePlane struct {
	phase v1.PodPhase
	plane string
}

func TestMonitor_getControlPlanePodState(t *testing.T) {
	tests := []struct {
		name                string
		expectedVizierPhase pixiev1alpha1.VizierPhase
		expectedReason      status.VizierReason
		podPhases           map[string]phasePlane
	}{
		{
			name:                "healthy",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodRunning,
					plane: "control",
				},
				"kelvin": {
					phase: v1.PodRunning,
					plane: "data",
				},
			},
			expectedReason: "",
		},
		{
			name:                "pending metadata",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUpdating,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodPending,
					plane: "control",
				},
				"vizier-query-broker": {
					phase: v1.PodRunning,
					plane: "control",
				},
				"kelvin": {
					phase: v1.PodRunning,
					plane: "data",
				},
			},
			expectedReason: status.ControlPlanePodsPending,
		},
		{
			name:                "healthy even if data plane pod is pending ",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodRunning,
					plane: "control",
				},
				"kelvin": {
					phase: v1.PodPending,
					plane: "data",
				},
			},
			expectedReason: "",
		},
		{
			name:                "updating if no-plane-pod is pending",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUpdating,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodRunning,
					plane: "control",
				},
				"kelvin": {
					phase: v1.PodRunning,
					plane: "data",
				},
				"no-plane-pod": {
					phase: v1.PodPending,
					plane: "",
				},
			},
			expectedReason: status.ControlPlanePodsPending,
		},
		{
			name:                "unhealthy if any control pod is failing",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodRunning,
					plane: "control",
				},
				"vizier-query-broker": {
					phase: v1.PodFailed,
					plane: "control",
				},
				"kelvin": {
					phase: v1.PodRunning,
					plane: "data",
				},
			},
			expectedReason: status.ControlPlanePodsFailed,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWithEvents)}
			for podLabel, fp := range test.podPhases {
				labels := make(map[string]string)
				if fp.plane != "" {
					labels["plane"] = fp.plane
				}

				pods.write(podLabel, podLabel, &podWithEvents{pod: &v1.Pod{
					ObjectMeta: metav1.ObjectMeta{
						Labels: labels,
					},
					Status: v1.PodStatus{
						Phase: fp.phase,
					},
				}})
			}

			state := getControlPlanePodState(pods)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}

func TestMonitor_getPEMsSomeInsufficientMemory(t *testing.T) {
	insufficientMemoryEvent := v1.Event{
		Type:    "Warning",
		Reason:  "FailedScheduling",
		Message: "0/2 nodes are available: 2 Insufficient memory.",
	}
	tests := []struct {
		name                string
		expectedVizierPhase pixiev1alpha1.VizierPhase
		expectedReason      status.VizierReason
		pems                []struct {
			name   string
			phase  v1.PodPhase
			events []v1.Event
		}
	}{
		{
			name:                "healthy",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			pems: []struct {
				name   string
				phase  v1.PodPhase
				events []v1.Event
			}{
				{
					name:  "vizier-pem-abcdefg",
					phase: v1.PodRunning,
				},
				{
					name:  "vizier-pem-123456",
					phase: v1.PodRunning,
				},
			},
			expectedReason: "",
		},
		{
			name:                "no pems",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			expectedReason:      status.PEMsMissing,
		},
		{
			name:                "degraded if some (not all) are insufficient memory",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseDegraded,
			pems: []struct {
				name   string
				phase  v1.PodPhase
				events []v1.Event
			}{
				{
					name:  "vizier-pem-abcdefg",
					phase: v1.PodRunning,
				},
				{
					name:  "vizier-pem-123456",
					phase: v1.PodPending,
					events: []v1.Event{
						insufficientMemoryEvent,
					},
				},
				{
					name:  "vizier-pem-zyx987",
					phase: v1.PodPending,
					events: []v1.Event{
						insufficientMemoryEvent,
					},
				},
			},
			expectedReason: status.PEMsSomeInsufficientMemory,
		},
		{
			name:                "unhealthy if all are insufficient memory",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			pems: []struct {
				name   string
				phase  v1.PodPhase
				events []v1.Event
			}{
				{
					name:  "vizier-pem-abcdefg",
					phase: v1.PodPending,
					events: []v1.Event{
						insufficientMemoryEvent,
					},
				},
				{
					name:  "vizier-pem-123456",
					phase: v1.PodPending,
					events: []v1.Event{
						insufficientMemoryEvent,
					},
				},
			},
			expectedReason: status.PEMsAllInsufficientMemory,
		},
		{
			name:                "pod pending for unrelated reason",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			pems: []struct {
				name   string
				phase  v1.PodPhase
				events []v1.Event
			}{
				{
					name:  "vizier-pem-abcdefg",
					phase: v1.PodPending,
					events: []v1.Event{
						v1.Event{
							Reason:  "FailedScheduling",
							Message: "foobar",
						},
					},
				},
			},
			expectedReason: "",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pems := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWithEvents)}
			for _, p := range test.pems {
				pems.write(vizierPemLabel, p.name, &podWithEvents{
					pod: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{Name: p.name},
						Status: v1.PodStatus{
							Phase: p.phase,
						},
					},
					events: p.events,
				})
			}

			state := getPEMResourceLimitsState(pems)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}
