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

package bridge

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/kubernetes/fake"
)

func runningContainer() corev1.ContainerStatus {
	return corev1.ContainerStatus{
		Name:  "app",
		Ready: true,
		State: corev1.ContainerState{
			Running: &corev1.ContainerStateRunning{},
		},
	}
}

func crashLoopingContainer() corev1.ContainerStatus {
	return corev1.ContainerStatus{
		Name:         "app",
		Ready:        false,
		RestartCount: 4,
		State: corev1.ContainerState{
			Waiting: &corev1.ContainerStateWaiting{Reason: "CrashLoopBackOff"},
		},
	}
}

func makeDataPlanePod(name, nameLabel string, phase corev1.PodPhase, containers ...corev1.ContainerStatus) *corev1.Pod {
	return &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      name,
			Namespace: "pl",
			Labels:    map[string]string{"name": nameLabel},
		},
		Status: corev1.PodStatus{
			Phase:             phase,
			ContainerStatuses: containers,
		},
	}
}

func TestIsDataPlanePodHealthy(t *testing.T) {
	tests := []struct {
		name     string
		pod      *corev1.Pod
		expected bool
	}{
		{
			name:     "running with ready containers",
			pod:      makeDataPlanePod("kelvin", "kelvin", corev1.PodRunning, runningContainer()),
			expected: true,
		},
		{
			name:     "running with crash looping container",
			pod:      makeDataPlanePod("kelvin", "kelvin", corev1.PodRunning, crashLoopingContainer()),
			expected: false,
		},
		{
			name:     "running with one of multiple containers not ready",
			pod:      makeDataPlanePod("kelvin", "kelvin", corev1.PodRunning, runningContainer(), crashLoopingContainer()),
			expected: false,
		},
		{
			name:     "pending",
			pod:      makeDataPlanePod("kelvin", "kelvin", corev1.PodPending),
			expected: false,
		},
		{
			name:     "failed",
			pod:      makeDataPlanePod("kelvin", "kelvin", corev1.PodFailed),
			expected: false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			assert.Equal(t, test.expected, isDataPlanePodHealthy(test.pod))
		})
	}
}

func TestGetDataPlaneState(t *testing.T) {
	tests := []struct {
		name                         string
		numNodes                     int32
		pods                         []*corev1.Pod
		expectedNumInstrumentedNodes int32
		expectedUnhealthyPods        []string
	}{
		{
			name:     "all healthy",
			numNodes: 2,
			pods: []*corev1.Pod{
				makeDataPlanePod("kelvin-0", "kelvin", corev1.PodRunning, runningContainer()),
				makeDataPlanePod("vizier-pem-0", "vizier-pem", corev1.PodRunning, runningContainer()),
				makeDataPlanePod("vizier-pem-1", "vizier-pem", corev1.PodRunning, runningContainer()),
			},
			expectedNumInstrumentedNodes: 2,
			expectedUnhealthyPods:        []string{},
		},
		{
			name:     "crash looping kelvin",
			numNodes: 2,
			pods: []*corev1.Pod{
				makeDataPlanePod("kelvin-0", "kelvin", corev1.PodRunning, crashLoopingContainer()),
				makeDataPlanePod("vizier-pem-0", "vizier-pem", corev1.PodRunning, runningContainer()),
				makeDataPlanePod("vizier-pem-1", "vizier-pem", corev1.PodRunning, runningContainer()),
			},
			expectedNumInstrumentedNodes: 2,
			expectedUnhealthyPods:        []string{"kelvin-0"},
		},
		{
			name:     "unhealthy pems",
			numNodes: 3,
			pods: []*corev1.Pod{
				makeDataPlanePod("kelvin-0", "kelvin", corev1.PodRunning, runningContainer()),
				makeDataPlanePod("vizier-pem-0", "vizier-pem", corev1.PodRunning, runningContainer()),
				makeDataPlanePod("vizier-pem-1", "vizier-pem", corev1.PodRunning, crashLoopingContainer()),
				makeDataPlanePod("vizier-pem-2", "vizier-pem", corev1.PodPending),
			},
			expectedNumInstrumentedNodes: 1,
			expectedUnhealthyPods:        []string{"vizier-pem-1", "vizier-pem-2"},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			var objs []runtime.Object
			for i := int32(0); i < test.numNodes; i++ {
				objs = append(objs, &corev1.Node{ObjectMeta: metav1.ObjectMeta{Name: fmt.Sprintf("node-%d", i)}})
			}
			for _, p := range test.pods {
				objs = append(objs, p)
			}
			vzInfo := &K8sVizierInfo{
				ns:        "pl",
				clientset: fake.NewSimpleClientset(objs...),
			}

			numNodes, numInstrumentedNodes, unhealthyPods, err := vzInfo.getDataPlaneState()
			require.NoError(t, err)
			assert.Equal(t, test.numNodes, numNodes)
			assert.Equal(t, test.expectedNumInstrumentedNodes, numInstrumentedNodes)

			unhealthyPodNames := make([]string, 0)
			for name := range unhealthyPods {
				unhealthyPodNames = append(unhealthyPodNames, name)
			}
			assert.ElementsMatch(t, test.expectedUnhealthyPods, unhealthyPodNames)
		})
	}
}

func TestGetDataPlaneState_LimitsUnhealthyPods(t *testing.T) {
	objs := []runtime.Object{
		&corev1.Node{ObjectMeta: metav1.ObjectMeta{Name: "node-0"}},
		makeDataPlanePod("kelvin-0", "kelvin", corev1.PodRunning, crashLoopingContainer()),
	}
	for i := 0; i < 15; i++ {
		objs = append(objs, makeDataPlanePod(fmt.Sprintf("vizier-pem-%02d", i), "vizier-pem", corev1.PodRunning, crashLoopingContainer()))
	}
	vzInfo := &K8sVizierInfo{
		ns:        "pl",
		clientset: fake.NewSimpleClientset(objs...),
	}

	_, numInstrumentedNodes, unhealthyPods, err := vzInfo.getDataPlaneState()
	require.NoError(t, err)
	assert.Equal(t, int32(0), numInstrumentedNodes)

	// The Kelvin pod is always reported, and the remaining slots are filled with the first PEMs by name.
	require.Len(t, unhealthyPods, 10)
	assert.Contains(t, unhealthyPods, "kelvin-0")
	for i := 0; i < 9; i++ {
		assert.Contains(t, unhealthyPods, fmt.Sprintf("vizier-pem-%02d", i))
	}
}
