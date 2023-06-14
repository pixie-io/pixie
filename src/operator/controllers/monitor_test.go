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
	"context"
	"fmt"
	"io"
	"net/http"
	"testing"
	"time"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	appsv1 "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	k8stypes "k8s.io/apimachinery/pkg/types"
	testclient "k8s.io/client-go/kubernetes/fake"
	k8stesting "k8s.io/client-go/testing"
	"sigs.k8s.io/controller-runtime/pkg/client"

	"px.dev/pixie/src/api/proto/cloudpb"
	mock_cloudpb "px.dev/pixie/src/api/proto/cloudpb/mock"
	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
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
			Body:       io.NopCloser(bytes.NewBufferString(resp)),
		}, nil
	}

	return &http.Response{
		Status:     "404",
		StatusCode: 404,
		Body:       io.NopCloser(bytes.NewBufferString("")),
	}, nil
}

func TestMonitor_queryPodStatusz(t *testing.T) {
	httpClient := &FakeHTTPClient{
		responses: map[string]string{
			"https://127-0-0-1.pl.pod.cluster.local:8080/statusz":   "",
			"https://127-0-0-3.pl2.pod.cluster.local:50100/statusz": "CloudConnectFailed",
		},
	}

	tests := []struct {
		name           string
		podPort        int32
		podIP          string
		podNamespace   string
		expectedStatus string
		expectedOK     bool
	}{
		{
			name:           "OK",
			podPort:        8080,
			podIP:          "127.0.0.1",
			podNamespace:   "pl",
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
			podNamespace:   "pl2",
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
				ObjectMeta: metav1.ObjectMeta{
					Namespace: test.podNamespace,
				},
				Spec: v1.PodSpec{
					Containers: []v1.Container{
						{
							Ports: []v1.ContainerPort{
								{
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
		expectedVizierPhase v1alpha1.VizierPhase
		expectedReason      status.VizierReason
	}{
		{
			name:                "healthy",
			cloudConnStatusz:    "",
			cloudConnPhase:      v1.PodRunning,
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
			expectedReason:      "",
		},
		{
			name:                "updating",
			cloudConnStatusz:    "",
			cloudConnPhase:      v1.PodPending,
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			expectedReason:      status.CloudConnectorPodPending,
		},
		{
			name:                "unhealthy but running",
			cloudConnStatusz:    "CloudConnectFailed",
			cloudConnPhase:      v1.PodRunning,
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			expectedReason:      status.CloudConnectorFailedToConnect,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			httpClient := &FakeHTTPClient{
				responses: map[string]string{
					"https://127-0-0-1.pl.pod.cluster.local:8080/statusz": test.cloudConnStatusz,
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
						ObjectMeta: metav1.ObjectMeta{
							Namespace: "pl",
						},
						Spec: v1.PodSpec{
							Containers: []v1.Container{
								{
									Ports: []v1.ContainerPort{
										{
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
			assert.Equal(t, test.expectedVizierPhase, v1alpha1.ReasonToPhase(state.Reason))
		})
	}
}

// The following is a test for getStatefulMetadataPendingState.
// It should test the following cases:
// 1. Vizier metadata pod with statefulset is pending and initContainers are complete: MetadataStatefulSetPodPending
// 2. Vizier metadata pod with statefulset is pending and initContainers are not complete: empty
// 3. Vizier metadata pod with statefulset is not pending: empty
// 4. Vizier metadata pod is owned by a replicaset: empty

func TestMonitor_getStatefulMetadataPendingState(t *testing.T) {
	tests := []struct {
		name           string
		pod            *v1.Pod
		lastTime       time.Time
		expectedReason status.VizierReason
	}{
		{
			name: "Vizier metadata pod with statefulset is pending and initContainers are complete",
			pod: &v1.Pod{
				Status: v1.PodStatus{
					Phase: v1.PodPending,
					InitContainerStatuses: []v1.ContainerStatus{
						{
							State: v1.ContainerState{
								Terminated: &v1.ContainerStateTerminated{
									ExitCode: 0,
								},
							},
						},
					},
				},
				ObjectMeta: metav1.ObjectMeta{
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "StatefulSet",
						},
					},
				},
			},
			lastTime:       time.Now().Add(-10 * time.Minute),
			expectedReason: status.MetadataStatefulSetPodPending,
		},
		{
			name: "Vizier metadata pod with statefulset is pending and initContainers are complete, but timeout not hit",
			pod: &v1.Pod{
				Status: v1.PodStatus{
					Phase: v1.PodPending,
					InitContainerStatuses: []v1.ContainerStatus{
						{
							State: v1.ContainerState{
								Terminated: &v1.ContainerStateTerminated{
									ExitCode: 0,
								},
							},
						},
					},
				},
				ObjectMeta: metav1.ObjectMeta{
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "StatefulSet",
						},
					},
				},
			},
			lastTime:       time.Now(),
			expectedReason: "",
		},
		{
			name: "Vizier metadata pod with statefulset is pending and initContainers are not complete",
			pod: &v1.Pod{
				Status: v1.PodStatus{
					Phase: v1.PodPending,
					InitContainerStatuses: []v1.ContainerStatus{
						{
							State: v1.ContainerState{
								Terminated: &v1.ContainerStateTerminated{
									ExitCode: 1,
								},
							},
						},
					},
				},
				ObjectMeta: metav1.ObjectMeta{
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "StatefulSet",
						},
					},
				},
			},
			lastTime:       time.Now().Add(-10 * time.Minute),
			expectedReason: "",
		},
		{
			name: "Vizier metadata pod with statefulset is not pending",
			pod: &v1.Pod{
				Status: v1.PodStatus{
					Phase: v1.PodRunning,
					InitContainerStatuses: []v1.ContainerStatus{
						{
							State: v1.ContainerState{
								Terminated: &v1.ContainerStateTerminated{
									ExitCode: 0,
								},
							},
						},
					},
				},
				ObjectMeta: metav1.ObjectMeta{
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "StatefulSet",
						},
					},
				},
			},
			lastTime:       time.Now().Add(-10 * time.Minute),
			expectedReason: "",
		},
		{
			name: "Vizier metadata pod with statefulset is not pending",
			pod: &v1.Pod{
				Status: v1.PodStatus{
					Phase: v1.PodPending,
					InitContainerStatuses: []v1.ContainerStatus{
						{
							State: v1.ContainerState{
								Terminated: &v1.ContainerStateTerminated{
									ExitCode: 0,
								},
							},
						},
					},
				},
				ObjectMeta: metav1.ObjectMeta{
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "Replicaset",
						},
					},
				},
			},
			lastTime:       time.Now().Add(-10 * time.Minute),
			expectedReason: "",
		},
	}

	for _, test := range tests {
		pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}
		pods.write(
			"vizier-metadata",
			"vizier-metadata-0",
			&podWrapper{
				pod: test.pod,
			})
		lastTime := metav1.NewTime(test.lastTime)
		state := getStatefulMetadataPendingState(pods, &v1alpha1.Vizier{
			Status: v1alpha1.VizierStatus{
				LastReconciliationPhaseTime: &lastTime,
			},
		})
		assert.Equal(t, test.expectedReason, state.Reason)
	}
}

func TestMonitor_repairVizier_NATS(t *testing.T) {
	tests := []struct {
		name               string
		podName            string
		state              *vizierState
		expectedError      string
		expectedDeleteCall string
	}{
		{
			name:               "natsPodFailed and correct name",
			podName:            "pl-nats-0",
			state:              &vizierState{Reason: status.NATSPodFailed},
			expectedError:      "",
			expectedDeleteCall: "pl-nats-0",
		},
		{
			name:               "natsPodFailed and incorrect name",
			podName:            "pl-nats-fail",
			state:              &vizierState{Reason: status.NATSPodFailed},
			expectedError:      "not found",
			expectedDeleteCall: "pl-nats-fail",
		},
		{
			name:               "natsPodPending and correct name",
			podName:            "pl-nats-0",
			state:              &vizierState{Reason: status.NATSPodPending},
			expectedError:      "",
			expectedDeleteCall: "",
		},
		{
			name:               "natsPod is running and correct name",
			podName:            "pl-nats-0",
			state:              &vizierState{Reason: ""},
			expectedError:      "",
			expectedDeleteCall: "",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			testPod := &v1.Pod{
				TypeMeta: metav1.TypeMeta{
					Kind:       "Pod",
					APIVersion: "v1",
				},
				ObjectMeta: metav1.ObjectMeta{
					Name:      test.podName,
					Namespace: "pl-nats",
				},
				Spec: v1.PodSpec{
					Containers: []v1.Container{
						{
							Name:            "nginx",
							Image:           "nginx",
							ImagePullPolicy: "Always",
						},
					},
				},
			}

			// Setting deleteCall to an empty string and changing it to test.podName if a delete action is called
			deleteCall := ""

			cs := testclient.NewSimpleClientset(testPod)
			cs.PrependReactor("*", "*", func(action k8stesting.Action) (bool, runtime.Object, error) {
				switch action := action.(type) {
				case k8stesting.DeleteActionImpl:
					// Records name of the pod sent to the delete function
					deleteCall = action.Name
					return false, nil, nil

				default:
					return false, nil, fmt.Errorf("No reaction implemented for %s", action)
				}
			})

			monitor := &VizierMonitor{clientset: cs, namespace: "pl-nats"}
			err := monitor.repairVizier(test.state)

			if test.expectedError != "" {
				assert.ErrorContains(t, err, test.expectedError)
			} else {
				assert.Nil(t, err)
				assert.Regexp(t, test.expectedDeleteCall, deleteCall)
			}
		})
	}
}

func TestMonitor_repairVizier_PVC(t *testing.T) {
	tests := []struct {
		name         string
		state        *vizierState
		updateCalled bool
	}{
		{
			name:         "MetadataPVCStorageClassUnavailable",
			state:        &vizierState{Reason: status.MetadataPVCStorageClassUnavailable},
			updateCalled: true,
		},
		{
			name:         "StateNotHandled",
			state:        &vizierState{Reason: status.CloudConnectorPodFailed},
			updateCalled: false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			cs := testclient.NewSimpleClientset()

			checkUpdateCall := false
			update := func(ctx context.Context, obj client.Object, opts ...client.UpdateOption) error {
				checkUpdateCall = true
				return nil
			}

			get := func(ctx context.Context, namespacedName k8stypes.NamespacedName, obj client.Object, opts ...client.GetOption) error {
				return nil
			}

			monitor := &VizierMonitor{clientset: cs, namespace: "pl-nats", vzGet: get, vzSpecUpdate: update}

			err := monitor.repairVizier(test.state)
			assert.Equal(t, test.updateCalled, checkUpdateCall)
			assert.Nil(t, err)
		})
	}
}

func TestMonitor_getCloudConnState_SeveralCloudConns(t *testing.T) {
	httpClient := &FakeHTTPClient{
		responses: map[string]string{},
	}

	pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}

	spec := v1.PodSpec{
		Containers: []v1.Container{
			{
				Ports: []v1.ContainerPort{
					{
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
	assert.Equal(t, v1alpha1.VizierPhaseUnhealthy, v1alpha1.ReasonToPhase(state.Reason))
}

func TestMonitor_NATSPods(t *testing.T) {
	httpClient := &FakeHTTPClient{
		responses: map[string]string{
			"http://127-0-0-1.pl.pod.cluster.local:8222": "",
			"http://127-0-0-3.pl.pod.cluster.local:8222": "NATS Failed",
		},
	}

	tests := []struct {
		name                string
		podMissing          bool
		natsIP              string
		natsPhase           v1.PodPhase
		expectedReason      status.VizierReason
		expectedVizierPhase v1alpha1.VizierPhase
		// The metadata label part of the pod that this is a part of.
		natsPodNameLabel string
		// The name of the nats pod.
		natsPodName string
	}{
		{
			name:                "OK",
			natsIP:              "127.0.0.1",
			natsPhase:           v1.PodRunning,
			expectedReason:      "",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
			natsPodNameLabel:    natsLabel,
			natsPodName:         natsPodName,
		},
		{
			name:                "pending",
			natsIP:              "127.0.0.2",
			natsPhase:           v1.PodPending,
			expectedReason:      "NATSPodPending",
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			natsPodNameLabel:    natsLabel,
			natsPodName:         natsPodName,
		},
		{
			name:                "missing",
			podMissing:          true,
			expectedReason:      "NATSPodMissing",
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
		},
		{
			name:                "unhealthy",
			natsIP:              "127.0.0.3",
			natsPhase:           v1.PodRunning,
			expectedReason:      "NATSPodFailed",
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			natsPodNameLabel:    natsLabel,
			natsPodName:         natsPodName,
		},
		{
			name:                "missing if nats pod is not named and label is natsLabel",
			expectedReason:      "NATSPodMissing",
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			natsPodNameLabel:    natsLabel,
			natsPodName:         "random_pod",
		},
		{
			name:                "missing if nats pod is not named and label is empty",
			expectedReason:      "NATSPodMissing",
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			natsPodNameLabel:    "",
			natsPodName:         "random_pod",
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pods := &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}
			if !test.podMissing {
				pods.write(
					test.natsPodNameLabel,
					test.natsPodName,
					&podWrapper{
						pod: &v1.Pod{
							Status: v1.PodStatus{
								PodIP: test.natsIP,
								Phase: test.natsPhase,
							},
							ObjectMeta: metav1.ObjectMeta{
								Namespace: "pl",
							},
						},
					},
				)
			}

			state := getNATSState(httpClient, pods)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, v1alpha1.ReasonToPhase(state.Reason))
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
		expectedVizierPhase v1alpha1.VizierPhase
		expectedReason      status.VizierReason
		podPhases           map[string]phasePlane
	}{
		{
			name:                "healthy",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
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
			name:                "healthy even if no-plane-pod is pending",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
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
			expectedReason: "",
		},
		{
			name:                "unhealthy if any control pod is failing",
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodPending,
					plane: "control",
					conditions: []v1.PodCondition{
						{
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
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			podPhases: map[string]phasePlane{
				"vizier-metadata": {
					phase: v1.PodPending,
					plane: "control",
					conditions: []v1.PodCondition{
						{
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
			assert.Equal(t, test.expectedVizierPhase, v1alpha1.ReasonToPhase(state.Reason))
		})
	}
}

var healthyConditions = []v1.PodCondition{
	{
		Type:   v1.PodReady,
		Status: v1.ConditionTrue,
	},
	{
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
		expectedVizierPhase v1alpha1.VizierPhase
		expectedReason      status.VizierReason
		pems                []pem
	}{
		{
			name:                "healthy",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
			expectedReason:      status.PEMsMissing,
		},
		{
			name:                "degraded if some (not all) are insufficient memory",
			expectedVizierPhase: v1alpha1.VizierPhaseDegraded,
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
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
			pems: []pem{
				{
					name:  "vizier-pem-abcdefg",
					phase: v1.PodPending,
					conditions: []v1.PodCondition{
						{
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
			assert.Equal(t, test.expectedVizierPhase, v1alpha1.ReasonToPhase(state.Reason))
		})
	}
}

func TestMonitor_getVizierVersionState(t *testing.T) {
	tests := []struct {
		name                string
		latestVersion       string
		currentVersion      string
		expectedReason      status.VizierReason
		expectedVizierPhase v1alpha1.VizierPhase
	}{
		{
			name:                "up-to-date",
			latestVersion:       "0.5.6",
			currentVersion:      "0.5.6",
			expectedReason:      "",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "same minor",
			latestVersion:       "0.5.8",
			currentVersion:      "0.5.6",
			expectedReason:      "",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "one minor less",
			latestVersion:       "0.5.6",
			currentVersion:      "0.4.6",
			expectedReason:      "",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "major not matching",
			latestVersion:       "1.5.6",
			currentVersion:      "0.5.6",
			expectedReason:      status.VizierVersionTooOld,
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
		},
		{
			name:                "two minor less",
			latestVersion:       "0.5.6",
			currentVersion:      "0.3.6",
			expectedReason:      status.VizierVersionTooOld,
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
		},
		{
			name:                "two minor less with patch",
			latestVersion:       "0.5.6",
			currentVersion:      "0.3.6-pre-r0.45",
			expectedReason:      status.VizierVersionTooOld,
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
		},
		{
			name:                "dev version",
			latestVersion:       "0.5.6",
			currentVersion:      "0.0.0-dev+Modified.0000000.19700101000000.0",
			expectedReason:      "",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
		},
		{
			name:                "same minor with patch",
			latestVersion:       "0.5.8",
			currentVersion:      "0.5.6-pre-r0.45",
			expectedReason:      "",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
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

			versionState := getVizierVersionState(ats, &v1alpha1.Vizier{
				Status: v1alpha1.VizierStatus{
					Version: test.currentVersion,
				},
			})

			assert.Equal(t, test.expectedReason, versionState.Reason)
			assert.Equal(t, test.expectedVizierPhase, v1alpha1.ReasonToPhase(versionState.Reason))
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
		expectedVizierPhase v1alpha1.VizierPhase
		expectedReason      status.VizierReason
		pems                []struct {
			name           string
			phase          v1.PodPhase
			containerState v1.ContainerState
		}
	}{
		{
			name:                "healthy",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseDegraded,
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
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
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
			expectedVizierPhase: v1alpha1.VizierPhaseUnhealthy,
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
								{
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
			assert.Equal(t, test.expectedVizierPhase, v1alpha1.ReasonToPhase(state.Reason))
		})
	}
}

func makePod(name string, phase v1.PodPhase, ownerKind string) *v1.Pod {
	return &v1.Pod{
		TypeMeta: metav1.TypeMeta{
			Kind:       "Pod",
			APIVersion: "v1",
		},
		ObjectMeta: metav1.ObjectMeta{
			Name:      name,
			Namespace: "pl",
			Labels: map[string]string{
				"name": "vizier-metadata",
			},
			OwnerReferences: []metav1.OwnerReference{
				{
					APIVersion: "apps/v1",
					Kind:       ownerKind,
					Name:       "vizier-metadata",
				},
			},
		},
		Status: v1.PodStatus{
			Phase: phase,
		},
		Spec: v1.PodSpec{
			Containers: []v1.Container{
				{
					Name:            "nginx",
					Image:           "nginx",
					ImagePullPolicy: "Always",
				},
			},
		},
	}
}

func TestMonitor_repairVizier_consolidateVizierDeployments(t *testing.T) {
	tests := []struct {
		name  string
		state *vizierState
		pods  []runtime.Object
		// hasEtcdOperator is the state of the metadata backed system before the repair.
		hasEtcdOperator bool
		// repairCallsUpdate is used to check if the vizier deployment should be updated by repairVizier.
		repairCallsUpdate bool
		// forceUpdate is used to force the update of the vizier deployment by setting status.
		forceUpdate bool
	}{
		{
			name:  "UseEtcdOperator - persistent running defaults to persistent",
			state: &vizierState{Reason: status.ControlPlanePodsPending},
			pods: []runtime.Object{
				&appsv1.Deployment{
					ObjectMeta: metav1.ObjectMeta{
						Name:      "vizier-metadata",
						Namespace: "pl",
					},
				},
				&appsv1.StatefulSet{
					ObjectMeta: metav1.ObjectMeta{
						Name:      "vizier-metadata",
						Namespace: "pl",
					},
				},
				makePod("vizier-metadata-0", v1.PodRunning, "StatefulSet"),
				makePod("vizier-metadata-9dk39aj", v1.PodPending, "Deployment"),
			},
			hasEtcdOperator:   true,
			repairCallsUpdate: false,
			forceUpdate:       false,
		},
		{
			name:  "!UseEtcdOperator - persistent running defaults to persistent",
			state: &vizierState{Reason: status.ControlPlanePodsPending},
			pods: []runtime.Object{
				&appsv1.Deployment{
					ObjectMeta: metav1.ObjectMeta{
						Name:      "vizier-metadata",
						Namespace: "pl",
					},
				},
				&appsv1.StatefulSet{
					ObjectMeta: metav1.ObjectMeta{
						Name:      "vizier-metadata",
						Namespace: "pl",
					},
				},
				makePod("vizier-metadata-0", v1.PodRunning, "StatefulSet"),
				makePod("vizier-metadata-9dk39aj", v1.PodRunning, "Deployment"),
			},
			hasEtcdOperator:   false,
			repairCallsUpdate: false,
			forceUpdate:       false,
		},
		{
			name:  "no problems if only stateful",
			state: &vizierState{Reason: status.ControlPlanePodsPending},
			pods: []runtime.Object{
				&appsv1.StatefulSet{
					ObjectMeta: metav1.ObjectMeta{
						Name:      "vizier-metadata",
						Namespace: "pl",
					},
				},
				makePod("vizier-metadata-0", v1.PodPending, "StatefulSet"),
			},
			hasEtcdOperator:   true,
			repairCallsUpdate: false,
			forceUpdate:       false,
		},
		{
			name:  "no problems if only deployment",
			state: &vizierState{Reason: status.ControlPlanePodsPending},
			pods: []runtime.Object{
				&appsv1.Deployment{
					ObjectMeta: metav1.ObjectMeta{
						Name:      "vizier-metadata",
						Namespace: "pl",
					},
				},
				makePod("vizier-metadata-9dk39aj", v1.PodPending, "Deployment"),
			},
			hasEtcdOperator:   true,
			repairCallsUpdate: false,
			forceUpdate:       false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			cs := testclient.NewSimpleClientset(test.pods...)
			get := func(ctx context.Context, namespacedName k8stypes.NamespacedName, obj client.Object, opts ...client.GetOption) error {
				vz := obj.(*v1alpha1.Vizier)
				vz.Spec.UseEtcdOperator = test.hasEtcdOperator
				vz.Status.Checksum = []byte("1234")
				return nil
			}
			callsSpecUpdate := false
			callsStatusUpdate := false
			specUpdate := func(ctx context.Context, obj client.Object, opts ...client.UpdateOption) error {
				callsSpecUpdate = true
				return nil
			}
			statusUpdate := func(ctx context.Context, obj client.Object, opts ...client.SubResourceUpdateOption) error {
				callsStatusUpdate = true
				return nil
			}
			monitor := &VizierMonitor{clientset: cs, vzGet: get, vzSpecUpdate: specUpdate, vzUpdate: statusUpdate, namespace: "pl"}
			err := monitor.repairVizier(test.state)
			assert.Equal(t, test.repairCallsUpdate, callsSpecUpdate || callsStatusUpdate)
			assert.Equal(t, test.forceUpdate, callsStatusUpdate)
			assert.Nil(t, err)
		})
	}
}
