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
	"testing"

	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"

	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/shared/status"
)

func TestMonitor_nodeWatcherHandleNode(t *testing.T) {
	tests := []struct {
		name                string
		initialState        nodeCompatTracker
		nodeKernel          string
		expectedReason      status.VizierReason
		expectedVizierPhase v1alpha1.VizierPhase
		delete              bool
	}{
		{
			name: "add compatible",
			initialState: nodeCompatTracker{
				numNodes:        3.0,
				numIncompatible: 1.0,
				kernelVersionDist: map[string]int{
					"5.0.1":  2,
					"4.13.0": 1,
				},
			},
			nodeKernel:          "4.14.0",
			expectedReason:      "",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
			delete:              false,
		},
		{
			name: "add incompatible",
			initialState: nodeCompatTracker{
				numNodes:        9.0,
				numIncompatible: 2.0,
				kernelVersionDist: map[string]int{
					"5.0.1":  7,
					"4.13.0": 2,
				},
			},
			nodeKernel:          "4.13.0",
			expectedReason:      status.KernelVersionsIncompatible,
			expectedVizierPhase: v1alpha1.VizierPhaseDegraded,
			delete:              false,
		},
		{
			name: "remove compatible",
			initialState: nodeCompatTracker{
				numNodes:        4.0,
				numIncompatible: 1.0,
				kernelVersionDist: map[string]int{
					"5.0.2":  3,
					"4.12.3": 1,
				},
			},
			nodeKernel:          "5.0.2",
			expectedReason:      status.KernelVersionsIncompatible,
			expectedVizierPhase: v1alpha1.VizierPhaseDegraded,
			delete:              true,
		},
		{
			name: "remove incompatible",
			initialState: nodeCompatTracker{
				numNodes:        10.0,
				numIncompatible: 3.0,
				kernelVersionDist: map[string]int{
					"5.0.1":  7,
					"4.12.3": 3,
				},
			},
			nodeKernel:          "4.12.3",
			expectedReason:      "",
			expectedVizierPhase: v1alpha1.VizierPhaseHealthy,
			delete:              true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			n := test.initialState
			initialCount := n.kernelVersionDist[test.nodeKernel]

			node := &v1.Node{
				Status: v1.NodeStatus{
					NodeInfo: v1.NodeSystemInfo{
						KernelVersion: test.nodeKernel,
					},
				},
			}

			if test.delete {
				n.removeNode(node)
			} else {
				n.addNode(node)
			}

			state := n.state()

			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, v1alpha1.ReasonToPhase(state.Reason))
			if test.delete {
				assert.Equal(t, initialCount-1, n.kernelVersionDist[test.nodeKernel])
			} else {
				assert.Equal(t, initialCount+1, n.kernelVersionDist[test.nodeKernel])
			}
		})
	}
}
