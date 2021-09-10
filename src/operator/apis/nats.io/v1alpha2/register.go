// Copyright 2017 The nats-operator Authors
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
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/runtime/schema"
)

const (
	CRDResourceKind   = "NatsCluster"
	CRDResourcePlural = "natsclusters"
	groupName         = "nats.io"

	ServiceRoleCRDResourceKind   = "NatsServiceRole"
	ServiceRoleCRDResourcePlural = "natsserviceroles"
)

var (
	SchemeBuilder = runtime.NewSchemeBuilder(addKnownTypes)
	AddToScheme   = SchemeBuilder.AddToScheme

	SchemeGroupVersion = schema.GroupVersion{Group: groupName, Version: "v1alpha2"}
	CRDName            = CRDResourcePlural + "." + groupName
	ServiceRoleCRDName = ServiceRoleCRDResourcePlural + "." + groupName
)

// Resource takes an unqualified resource and returns a group-qualified GroupResource.
func Resource(resource string) schema.GroupResource {
	return SchemeGroupVersion.WithResource(resource).GroupResource()
}

// addKnownTypes adds the set of types defined in this package to the supplied scheme.
func addKnownTypes(s *runtime.Scheme) error {
	s.AddKnownTypes(SchemeGroupVersion,
		&NatsCluster{},
		&NatsClusterList{},
		&NatsServiceRole{},
		&NatsServiceRoleList{},
	)
	metav1.AddToGroupVersion(s, SchemeGroupVersion)
	return nil
}
