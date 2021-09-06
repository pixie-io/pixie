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

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/api/proto/cloudpb"
	mock_cloudpb "px.dev/pixie/src/api/proto/cloudpb/mock"
	pixiev1alpha1 "px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
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

			pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}
			pods.write(
				"vizier-cloud-connector",
				"vizier-cloud-connector-abcdefg",
				&podWrapper{
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

	pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}

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
	pods.write("vizier-cloud-connector", "vizier-cloud-connector-abcdefg", &podWrapper{
		pod: &v1.Pod{
			Status: v1.PodStatus{
				PodIP: "127.0.0.1",
				Phase: v1.PodRunning,
			},
			Spec: spec,
		},
	})
	pods.write("vizier-cloud-connector", "vizier-cloud-connector-12345678", &podWrapper{
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
			pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}
			if !test.podMissing {
				pods.write(
					"",
					natsName,
					&podWrapper{
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

func TestMonitor_getControlPlanePodState(t *testing.T) {
	type phasePlane struct {
		phase      v1.PodPhase
		plane      string
		conditions []v1.PodCondition
	}
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
					phase:      v1.PodRunning,
					plane:      "control",
					conditions: healthyConditions,
				},
				"kelvin": {
					phase:      v1.PodRunning,
					plane:      "data",
					conditions: healthyConditions,
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
					phase:      v1.PodRunning,
					plane:      "control",
					conditions: healthyConditions,
				},
				"kelvin": {
					phase:      v1.PodRunning,
					plane:      "data",
					conditions: healthyConditions,
				},
			},
			expectedReason: status.ControlPlanePodsPending,
		},
		{
			name:                "healthy even if data plane pod is pending",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase:      v1.PodRunning,
					plane:      "control",
					conditions: healthyConditions,
				},
				"kelvin": {
					phase: v1.PodPending,
					plane: "data",
				},
			},
			expectedReason: "",
		},
		{
			name:                "healthy if control plane pod succeeded",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase:      v1.PodRunning,
					plane:      "control",
					conditions: healthyConditions,
				},
				"vizier-certmgr": {
					phase: v1.PodSucceeded,
					plane: "",
				},
			},
			expectedReason: "",
		},
		{
			name:                "updating if no-plane-pod is pending",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUpdating,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase:      v1.PodRunning,
					plane:      "control",
					conditions: healthyConditions,
				},
				"kelvin": {
					phase:      v1.PodRunning,
					plane:      "data",
					conditions: healthyConditions,
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
					phase:      v1.PodRunning,
					plane:      "control",
					conditions: healthyConditions,
				},
				"vizier-query-broker": {
					phase: v1.PodFailed,
					plane: "control",
				},
				"kelvin": {
					phase:      v1.PodRunning,
					plane:      "data",
					conditions: healthyConditions,
				},
			},
			expectedReason: status.ControlPlanePodsFailed,
		},
		{
			name:                "unhealthy if any control pod cant be scheduled because of tain",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodPending,
					plane: "control",
					conditions: []v1.PodCondition{
						v1.PodCondition{
							Type:    v1.PodScheduled,
							Status:  v1.ConditionFalse,
							Reason:  v1.PodReasonUnschedulable,
							Message: "0/2 nodes are available: 2 node(s) had taint {key1: value1}, that the pod didn't tolerate.",
						},
					},
				},
			},
			expectedReason: status.ControlPlaneFailedToScheduleBecauseOfTaints,
		},
		{
			name:                "unhealthy if any control pod cant be scheduled generic",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodPending,
					plane: "control",
					conditions: []v1.PodCondition{
						v1.PodCondition{
							Type:    v1.PodScheduled,
							Status:  v1.ConditionFalse,
							Reason:  v1.PodReasonUnschedulable,
							Message: "generic condition",
						},
					},
				},
			},
			expectedReason: status.ControlPlaneFailedToSchedule,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}
			for podLabel, p := range test.podPhases {
				labels := make(map[string]string)
				if p.plane != "" {
					labels["plane"] = p.plane
				}

				pods.write(podLabel, podLabel, &podWrapper{pod: &v1.Pod{
					ObjectMeta: metav1.ObjectMeta{
						Labels: labels,
					},
					Status: v1.PodStatus{
						Conditions: p.conditions,
						Phase:      p.phase,
					},
				}})
			}

			state := getControlPlanePodState(pods)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}

var healthyConditions = []v1.PodCondition{
	v1.PodCondition{
		Type:   v1.PodReady,
		Status: v1.ConditionTrue,
	},
	v1.PodCondition{
		Type:   v1.PodScheduled,
		Status: v1.ConditionTrue,
	},
}

var insufficientMemoryPodCondition = v1.PodCondition{
	Type:    v1.PodScheduled,
	Status:  v1.ConditionFalse,
	Reason:  v1.PodReasonUnschedulable,
	Message: "0/2 nodes are available: 2 Insufficient memory.",
}

func TestMonitor_getPEMsSomeInsufficientMemory(t *testing.T) {
	type pem struct {
		name       string
		phase      v1.PodPhase
		conditions []v1.PodCondition
	}
	tests := []struct {
		name                string
		expectedVizierPhase pixiev1alpha1.VizierPhase
		expectedReason      status.VizierReason
		pems                []pem
	}{
		{
			name:                "healthy",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			pems: []pem{
				{
					name:       "vizier-pem-abcdefg",
					phase:      v1.PodRunning,
					conditions: healthyConditions,
				},
				{
					name:       "vizier-pem-123456",
					phase:      v1.PodRunning,
					conditions: healthyConditions,
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
			pems: []pem{
				{
					name:       "vizier-pem-abcdefg",
					phase:      v1.PodRunning,
					conditions: healthyConditions,
				},
				{
					name:  "vizier-pem-123456",
					phase: v1.PodPending,
					conditions: []v1.PodCondition{
						insufficientMemoryPodCondition,
					},
				},
				{
					name:  "vizier-pem-zyx987",
					phase: v1.PodPending,
					conditions: []v1.PodCondition{
						insufficientMemoryPodCondition,
					},
				},
			},
			expectedReason: status.PEMsSomeInsufficientMemory,
		},
		{
			name:                "unhealthy if all are insufficient memory",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			pems: []pem{
				{
					name:  "vizier-pem-abcdefg",
					phase: v1.PodPending,
					conditions: []v1.PodCondition{
						insufficientMemoryPodCondition,
					},
				},
				{
					name:  "vizier-pem-123456",
					phase: v1.PodPending,
					conditions: []v1.PodCondition{
						insufficientMemoryPodCondition,
					},
				},
			},
			expectedReason: status.PEMsAllInsufficientMemory,
		},
		{
			name:                "pod pending for unrelated reason",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			pems: []pem{
				{
					name:  "vizier-pem-abcdefg",
					phase: v1.PodPending,
					conditions: []v1.PodCondition{
						v1.PodCondition{
							Reason:  "FailedScheduling",
							Message: "foo",
						},
					},
				},
			},
			expectedReason: "",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pems := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}
			for _, p := range test.pems {
				pems.write(vizierPemLabel, p.name, &podWrapper{
					pod: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{Name: p.name},
						Status: v1.PodStatus{
							Phase:      p.phase,
							Conditions: p.conditions,
						},
					},
				})
			}

			state := getPEMResourceLimitsState(pems)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}

func TestMonitor_getVizierVersionState(t *testing.T) {
	tests := []struct {
		name                string
		latestVersion       string
		currentVersion      string
		expectedReason      status.VizierReason
		expectedVizierPhase pixiev1alpha1.VizierPhase
	}{
		{
			name:                "up-to-date",
			latestVersion:       "0.5.6",
			currentVersion:      "0.5.6",
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "same minor",
			latestVersion:       "0.5.8",
			currentVersion:      "0.5.6",
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "one minor less",
			latestVersion:       "0.5.6",
			currentVersion:      "0.4.6",
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "major not matching",
			latestVersion:       "1.5.6",
			currentVersion:      "0.5.6",
			expectedReason:      status.VizierVersionTooOld,
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
		},
		{
			name:                "two minor less",
			latestVersion:       "0.5.6",
			currentVersion:      "0.3.6",
			expectedReason:      status.VizierVersionTooOld,
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
		},
		{
			name:                "two minor less with patch",
			latestVersion:       "0.5.6",
			currentVersion:      "0.3.6-pre-r0.45",
			expectedReason:      status.VizierVersionTooOld,
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
		},
		{
			name:                "dev version",
			latestVersion:       "0.5.6",
			currentVersion:      "0.0.0-dev+Modified.0000000.19700101000000.0",
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "same minor with patch",
			latestVersion:       "0.5.8",
			currentVersion:      "0.5.6-pre-r0.45",
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			ats := mock_cloudpb.NewMockArtifactTrackerClient(ctrl)

			ats.EXPECT().GetArtifactList(gomock.Any(),
				&cloudpb.GetArtifactListRequest{
					ArtifactName: "vizier",
					ArtifactType: cloudpb.AT_CONTAINER_SET_YAMLS,
					Limit:        1,
				}).
				Return(&cloudpb.ArtifactSet{
					Name: "vizier",
					Artifact: []*cloudpb.Artifact{{
						VersionStr: test.latestVersion,
						Timestamp:  &types.Timestamp{Seconds: 10},
					},
					},
				}, nil)

			versionState := getVizierVersionState(ats, &pixiev1alpha1.Vizier{
				Status: pixiev1alpha1.VizierStatus{
					Version: test.currentVersion,
				},
			})

			assert.Equal(t, test.expectedReason, versionState.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(versionState.Reason))
		})
	}
}

func TestMonitor_getPEMCrashingState(t *testing.T) {
	terminatedErrorState := v1.ContainerState{
		Terminated: &v1.ContainerStateTerminated{
			Reason: "Error",
		},
	}
	healthyContainerState := v1.ContainerState{Running: &v1.ContainerStateRunning{}}
	crashLoopBackoffState := v1.ContainerState{
		Waiting: &v1.ContainerStateWaiting{
			Reason: "CrashLoopBackOff",
		},
	}

	tests := []struct {
		name                string
		expectedVizierPhase pixiev1alpha1.VizierPhase
		expectedReason      status.VizierReason
		pems                []struct {
			name           string
			phase          v1.PodPhase
			containerState v1.ContainerState
		}
	}{
		{
			name:                "healthy",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			pems: []struct {
				name           string
				phase          v1.PodPhase
				containerState v1.ContainerState
			}{
				{
					name:           "vizier-pem-abcdefg",
					phase:          v1.PodRunning,
					containerState: healthyContainerState,
				},
				{
					name:           "vizier-pem-123456",
					phase:          v1.PodRunning,
					containerState: healthyContainerState,
				},
			},
			expectedReason: "",
		},
		{
			name:                "all crashing loop backoff",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			pems: []struct {
				name           string
				phase          v1.PodPhase
				containerState v1.ContainerState
			}{
				{
					name:           "vizier-pem-abcdefg",
					phase:          v1.PodRunning,
					containerState: crashLoopBackoffState,
				},
				{
					name:           "vizier-pem-123456",
					phase:          v1.PodRunning,
					containerState: crashLoopBackoffState,
				},
			},
			expectedReason: status.PEMsAllFailing,
		},
		{
			name:                "all terminated with error",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			pems: []struct {
				name           string
				phase          v1.PodPhase
				containerState v1.ContainerState
			}{
				{
					name:           "vizier-pem-abcdefg",
					phase:          v1.PodRunning,
					containerState: terminatedErrorState,
				},
				{
					name:           "vizier-pem-123456",
					phase:          v1.PodRunning,
					containerState: terminatedErrorState,
				},
			},
			expectedReason: status.PEMsAllFailing,
		},
		{
			name:                "degraded if some (not all) are failing",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseDegraded,
			pems: []struct {
				name           string
				phase          v1.PodPhase
				containerState v1.ContainerState
			}{
				{
					name:           "vizier-pem-abcdefg",
					phase:          v1.PodRunning,
					containerState: healthyContainerState,
				},
				{
					name:           "vizier-pem-123456",
					phase:          v1.PodRunning,
					containerState: crashLoopBackoffState,
				},
			},
			expectedReason: status.PEMsHighFailureRate,
		},
		{
			name:                "pending pods are ignored for this checker",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			pems: []struct {
				name           string
				phase          v1.PodPhase
				containerState v1.ContainerState
			}{
				{
					name:           "vizier-pem-abcdefg",
					phase:          v1.PodPending,
					containerState: healthyContainerState,
				},
				{
					name:           "vizier-pem-123456",
					phase:          v1.PodPending,
					containerState: crashLoopBackoffState,
				},
			},
			expectedReason: "",
		},
		{
			name:                "no pems",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseUnhealthy,
			expectedReason:      status.PEMsMissing,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pems := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}
			for _, p := range test.pems {
				pems.write(vizierPemLabel, p.name, &podWrapper{
					pod: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{Name: p.name},
						Status: v1.PodStatus{
							Phase: p.phase,
							ContainerStatuses: []v1.ContainerStatus{
								v1.ContainerStatus{
									Name:  "pem",
									State: p.containerState,
								},
							},
						},
					},
				})
			}

			state := getPEMCrashingState(pems)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}
