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
// Generate the CRD YAMLs.
//go:generate controller-gen crd:trivialVersions=true rbac:roleName=operator-role webhook output:crd:artifacts:config=crd output:crd:dir:=../../../../k8s/operator/crd/base

package v1alpha1

import (
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// VizierSpec defines the desired state of Vizier
type VizierSpec struct {
	// Version is the desired version of the Vizier instance.
	Version string `json:"version,omitempty"`
	// DeployKey is the deploy key associated with the Vizier instance. This is used to link the Vizier to a
	// specific user/org.
	DeployKey string `json:"deployKey"`
	// DisableAutoUpdate specifies whether auto update should be enabled for the Vizier instance.
	DisableAutoUpdate bool `json:"disableAutoUpdate,omitempty"`
	// UseEtcdOperator specifies whether the metadata service should use etcd for storage.
	UseEtcdOperator bool `json:"useEtcdOperator,omitempty"`
	// ClusterName is a name for the Vizier instance, usually specifying which cluster the Vizier is
	// deployed to. If not specified, a random name will be generated.
	ClusterName string `json:"clusterName,omitempty"`
	// CloudAddr is the address of the cloud instance that the Vizier should be pointing to.
	CloudAddr string `json:"cloudAddr,omitempty"`
	// DevCloudNamespace should be specified only for dev versions of Pixie cloud which have no ingress to help
	// redirect traffic to the correct service. The DevCloudNamespace is the namespace that the dev Pixie cloud is
	// running on, for example: "plc-dev".
	DevCloudNamespace string `json:"devCloudNamespace,omitempty"`
	// PemMemoryLimit is a memory limit applied specifically to PEM pods.
	PemMemoryLimit string `json:"pemMemoryLimit,omitempty"`
	// Pod defines the policy for creating Vizier pods.
	Pod *PodPolicy `json:"pod,omitempty"`
}

// VizierStatus defines the observed state of Vizier
type VizierStatus struct {
	// Version is the actual version of the Vizier instance.
	Version string `json:"version,omitempty"`
	// VizierPhase is a high-level summary of where the Vizier is in its lifecycle.
	VizierPhase VizierPhase `json:"vizierPhase,omitempty"`
	// Message is a human-readable message with details about why the Vizier is in this condition.
	Message string `json:"message,omitempty"`
}

// VizierPhase is a high-level summary of where the Vizier is in its lifecycle.
type VizierPhase string

const (
	// VizierPhaseNone indicates that the vizier phase is unknown.
	VizierPhaseNone VizierPhase = ""
	// VizierPhasePending indicates that the vizier is either in the process of creating or updating.
	VizierPhasePending = "Pending"
	// VizierPhaseRunning indicates that all vizier resources have been deployed.
	VizierPhaseRunning = "Running"
	// VizierPhaseFailed indicates that some vizier resources have failed to deploy.
	VizierPhaseFailed = "Failed"
)

// PodPolicy defines the policy for creating Vizier pods.
type PodPolicy struct {
	// Labels specifies the labels to attach to pods the operator creates.
	Labels map[string]string `json:"labels,omitempty"`
	// Annotations specifies the annotations to attach to pods the operator creates.
	Annotations map[string]string `json:"annotations,omitempty"`
	// Resources is the resource requirements for a container.
	// This field cannot be updated once the cluster is created.
	Resources v1.ResourceRequirements `json:"resources,omitempty"`
}

// +kubebuilder:object:root=true
// +kubebuilder:subresource:status
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
