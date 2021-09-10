// Copyright 2018 The nats-operator Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

package v1alpha2

import (
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// +k8s:deepcopy-gen:interfaces=k8s.io/apimachinery/pkg/runtime.Object
type NatsServiceRoleList struct {
	metav1.TypeMeta `json:",inline"`
	// Standard list metadata
	// More info: http://releases.k8s.io/HEAD/docs/devel/api-conventions.md#metadata
	metav1.ListMeta `json:"metadata,omitempty"`
	Items           []NatsServiceRole `json:"items"`
}

// ServiceRole is a NATS cluster service role. Its name is should be the same
// as the name a Kubernetes ServiceAccount in a namespace.
//
// +genclient
// +genclient:noStatus
// +k8s:deepcopy-gen:interfaces=k8s.io/apimachinery/pkg/runtime.Object
type NatsServiceRole struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata,omitempty"`
	Spec              ServiceRoleSpec `json:"spec"`
}

func (c *NatsServiceRole) AsOwner() metav1.OwnerReference {
	trueVar := true
	return metav1.OwnerReference{
		APIVersion: c.APIVersion,
		Kind:       c.Kind,
		Name:       c.Name,
		UID:        c.UID,
		Controller: &trueVar,
	}
}

// ServiceRoleSpec defines the permissions for a user account used by a NATS cluster client.
type ServiceRoleSpec struct {
	// Permissions are the authorization rules defined for a ServiceAccount.
	Permissions Permissions `json:"permissions,omitempty" protobuf:"bytes,1,opt,name=permissions"`
}

// Permissions are the authorization rules defined for a role.
type Permissions struct {
	Publish   []string `json:"publish,omitempty"`
	Subscribe []string `json:"subscribe,omitempty"`
}
