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
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	pixiev1alpha1 "px.dev/pixie/src/operator/api/v1alpha1"
	"px.dev/pixie/src/shared/status"
)

func TestMonitor_nodeWatcherHandleNode(t *testing.T) {
	tests := []struct {
		name                string
		existingState       map[string]bool
		newNodeName         string
		newNodeKernel       string
		expectedReason      status.VizierReason
		expectedVizierPhase pixiev1alpha1.VizierPhase
		delete              bool
	}{
		{
			name: "compatible",
			existingState: map[string]bool{
				"compatibleNode1":   true,
				"compatibleNode2":   true,
				"compatibleNode3":   true,
				"incompatibleNode1": false,
			},
			newNodeKernel:       "4.14.0",
			newNodeName:         "newCompatibleNode",
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			delete:              false,
		},
		{
			name: "incompatible",
			existingState: map[string]bool{
				"compatibleNode1":   true,
				"incompatibleNode3": true,
			},
			newNodeKernel:       "4.13.0",
			newNodeName:         "newIncompatibleNode",
			expectedReason:      status.KernelVersionsIncompatible,
			expectedVizierPhase: pixiev1alpha1.VizierPhaseDegraded,
			delete:              false,
		},
		{
			name: "update_incompat",
			existingState: map[string]bool{
				"compatibleNode1":   true,
				"incompatibleNode3": false,
			},
			newNodeKernel:       "4.13.0",
			newNodeName:         "compatibleNode1",
			expectedReason:      status.KernelVersionsIncompatible,
			expectedVizierPhase: pixiev1alpha1.VizierPhaseDegraded,
			delete:              false,
		},
		{
			name: "update_compat",
			existingState: map[string]bool{
				"compatibleNode1":   true,
				"incompatibleNode3": false,
			},
			newNodeKernel:       "4.14.1",
			newNodeName:         "incompatibleNode3",
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			delete:              false,
		},
		{
			name: "dead",
			existingState: map[string]bool{
				"compatibleNode1":   true,
				"incompatibleNode3": true,
				"incompatibleNode1": false,
			},
			newNodeKernel:       "4.13.0",
			newNodeName:         "incompatibleNode1",
			expectedReason:      "",
			expectedVizierPhase: pixiev1alpha1.VizierPhaseHealthy,
			delete:              true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			n := &nodeCompatTracker{
				incompatibleCount: 0.0,
				nodeCompatible:    test.existingState,
			}
			for _, c := range test.existingState {
				if !c {
					n.incompatibleCount++
				}
			}

			node := &v1.Node{
				ObjectMeta: metav1.ObjectMeta{
					Name: test.newNodeName,
				},
				Status: v1.NodeStatus{
					NodeInfo: v1.NodeSystemInfo{
						KernelVersion: test.newNodeKernel,
					},
				},
			}

			if test.delete {
				n.removeNode(node)
			} else if _, ok := test.existingState[node.Name]; ok {
				n.updateNode(node)
			} else {
				n.addNode(node)
			}

			state := n.state()

			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, translateReasonToPhase(state.Reason))
		})
	}
}
