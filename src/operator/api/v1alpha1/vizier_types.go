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

// Generate the code for deep-copying the CRD in go.
//go:generate controller-gen object
// Generate the CRD YAMLs. The generated crd/px.dev_viziers.yaml should be moved to k8s/operator/crd/base:
// `mv crd/px.dev_viziers.yaml ./k8s/operator/crd/base/px.dev_viziers.yaml`
//go:generate controller-gen crd:trivialVersions=true rbac:roleName=operator-role webhook output:crd:artifacts:config=crd

package v1alpha1

import (
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// VizierSpec defines the desired state of Vizier
type VizierSpec struct {
	// Version is the desired version of the Vizier instance.
	Version string `json:"version,omitempty"`
}

// VizierStatus defines the observed state of Vizier
type VizierStatus struct {
	// Version is the actual version of the Vizier instance.
	Version string `json:"version,omitempty"`
}

// +kubebuilder:object:root=true

// Vizier is the Schema for the viziers API
type Vizier struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata,omitempty"`

	Spec   VizierSpec   `json:"spec,omitempty"`
	Status VizierStatus `json:"status,omitempty"`
}

// +kubebuilder:object:root=true

// VizierList contains a list of Vizier
type VizierList struct {
	metav1.TypeMeta `json:",inline"`
	metav1.ListMeta `json:"metadata,omitempty"`
	Items           []Vizier `json:"items"`
}

func init() {
	SchemeBuilder.Register(&Vizier{}, &VizierList{})
}
