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
	storagev1 "k8s.io/api/storage/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes/fake"

	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/shared/status"
)

func TestMonitor_getPVCState(t *testing.T) {
	tests := []struct {
		name                     string
		pvcStorageClassName      string
		clusterStorageClassNames []string
		pvcPhase                 v1.PersistentVolumeClaimPhase
		expectedVizierPhase      v1alpha1.VizierPhase
		expectedReason           status.VizierReason
	}{
		{
			name:                     "healthy",
			pvcPhase:                 v1.ClaimBound,
			pvcStorageClassName:      "standard",
			clusterStorageClassNames: []string{"standard"},
			expectedVizierPhase:      v1alpha1.VizierPhaseHealthy,
			expectedReason:           "",
		},
		{
			name:                     "true_claim_pending",
			pvcPhase:                 v1.ClaimPending,
			pvcStorageClassName:      "standard",
			clusterStorageClassNames: []string{"standard"},
			expectedVizierPhase:      v1alpha1.VizierPhaseUnhealthy,
			expectedReason:           status.MetadataPVCPendingBinding,
		},
		{
			name:                     "storage class not available on cluster",
			pvcPhase:                 v1.ClaimPending,
			pvcStorageClassName:      "standard",
			clusterStorageClassNames: []string{},
			expectedVizierPhase:      v1alpha1.VizierPhaseUnhealthy,
			expectedReason:           status.MetadataPVCStorageClassUnavailable,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			pvc := &v1.PersistentVolumeClaim{
				Status: v1.PersistentVolumeClaimStatus{
					Phase: test.pvcPhase,
				},
				Spec: v1.PersistentVolumeClaimSpec{
					StorageClassName: &test.pvcStorageClassName,
				},
			}

			scList := &storagev1.StorageClassList{}
			for _, className := range test.clusterStorageClassNames {
				scList.Items = append(scList.Items, storagev1.StorageClass{
					ObjectMeta: metav1.ObjectMeta{Name: className},
				})
			}
			clientset := fake.NewSimpleClientset(scList)

			state := metadataPVCState(clientset, pvc)
			assert.Equal(t, test.expectedReason, state.Reason)
			assert.Equal(t, test.expectedVizierPhase, v1alpha1.ReasonToPhase(state.Reason))
		})
	}
}
