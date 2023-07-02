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

package k8smeta_test

import (
	"context"
	"errors"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	appsv1 "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/util/intstr"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/fake"
	clienttesting "k8s.io/client-go/testing"

	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/vizier/services/metadata/controllers/k8smeta"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
)

func int32ptr(i int32) *int32 {
	return &i
}

func intStr(str string) *intstr.IntOrString {
	is := intstr.FromString(str)
	return &is
}

func TestController(t *testing.T) {
	testCases := []struct {
		name            string
		updates         []resourceUpdate
		inits           []resourceUpdate
		expectedUpdates []*k8smeta.K8sResourceMessage
	}{
		{
			name: "simple pod",
			updates: []resourceUpdate{
				&pod{
					p: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mypod",
						},
						Spec: v1.PodSpec{
							Hostname: "hostname",
						},
					},
					ns: "test",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "pods",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Pod{
							Pod: &metadatapb.Pod{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mypod",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.PodSpec{
									Hostname: "hostname",
								},
								Status: &metadatapb.PodStatus{
									Conditions:        []*metadatapb.PodCondition{},
									ContainerStatuses: []*metadatapb.ContainerStatus{},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "pod create update delete",
			updates: []resourceUpdate{
				&pod{
					p: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mypod",
						},
						Spec: v1.PodSpec{
							Priority: int32ptr(0),
						},
					},
					ns: "test",
					t:  create,
				},
				&pod{
					p: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mypod",
						},
						Spec: v1.PodSpec{
							Priority: int32ptr(1),
						},
					},
					ns: "test",
					t:  update,
				},
				&pod{
					p: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mypod",
						},
					},
					ns: "test",
					t:  del,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "pods",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Pod{
							Pod: &metadatapb.Pod{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mypod",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.PodSpec{
									Priority: 0,
								},
								Status: &metadatapb.PodStatus{
									Conditions:        []*metadatapb.PodCondition{},
									ContainerStatuses: []*metadatapb.ContainerStatus{},
								},
							},
						},
					},
					EventType: watch.Added,
				},
				{
					ObjectType: "pods",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Pod{
							Pod: &metadatapb.Pod{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mypod",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.PodSpec{
									Priority: 1,
								},
								Status: &metadatapb.PodStatus{
									Conditions:        []*metadatapb.PodCondition{},
									ContainerStatuses: []*metadatapb.ContainerStatus{},
								},
							},
						},
					},
					EventType: watch.Modified,
				},
				{
					ObjectType: "pods",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Pod{
							Pod: &metadatapb.Pod{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mypod",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.PodSpec{
									Priority: 1,
								},
								Status: &metadatapb.PodStatus{
									Conditions:        []*metadatapb.PodCondition{},
									ContainerStatuses: []*metadatapb.ContainerStatus{},
								},
							},
						},
					},
					EventType: watch.Deleted,
				},
			},
		},
		{
			name: "pod before watcher",
			inits: []resourceUpdate{
				&pod{
					p: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mypod",
						},
						Spec: v1.PodSpec{
							Hostname: "hostname",
						},
					},
					ns: "test",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "pods",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Pod{
							Pod: &metadatapb.Pod{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mypod",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.PodSpec{
									Hostname: "hostname",
								},
								Status: &metadatapb.PodStatus{
									Conditions:        []*metadatapb.PodCondition{},
									ContainerStatuses: []*metadatapb.ContainerStatus{},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "simple node",
			updates: []resourceUpdate{
				&node{
					n: &v1.Node{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mynode",
						},
					},
					t: create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "nodes",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Node{
							Node: &metadatapb.Node{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mynode",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.NodeSpec{},
								Status: &metadatapb.NodeStatus{
									Conditions: []*metadatapb.NodeCondition{},
									Addresses:  []*metadatapb.NodeAddress{},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "node before watcher",
			inits: []resourceUpdate{
				&node{
					n: &v1.Node{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mynode",
						},
					},
					t: create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "nodes",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Node{
							Node: &metadatapb.Node{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mynode",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.NodeSpec{},
								Status: &metadatapb.NodeStatus{
									Conditions: []*metadatapb.NodeCondition{},
									Addresses:  []*metadatapb.NodeAddress{},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "simple namespace",
			updates: []resourceUpdate{
				&namespace{
					n: &v1.Namespace{
						ObjectMeta: metav1.ObjectMeta{
							Name: "myns",
						},
					},
					t: create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "namespaces",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Namespace{
							Namespace: &metadatapb.Namespace{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "myns",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "simple endpoint",
			updates: []resourceUpdate{
				&endpoints{
					e: &v1.Endpoints{
						ObjectMeta: metav1.ObjectMeta{
							Name: "myendpoint",
						},
						Subsets: []v1.EndpointSubset{
							{
								Addresses: []v1.EndpointAddress{
									{
										IP:       "127.0.0.1",
										Hostname: "hostname",
									},
								},
								NotReadyAddresses: []v1.EndpointAddress{
									{
										IP:       "127.0.0.2",
										Hostname: "notready",
									},
								},
								Ports: []v1.EndpointPort{
									{
										Name:     "endpointport",
										Protocol: v1.ProtocolTCP,
										Port:     8081,
									},
								},
							},
						},
					},
					ns: "test",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "endpoints",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Endpoints{
							Endpoints: &metadatapb.Endpoints{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "myendpoint",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Subsets: []*metadatapb.EndpointSubset{
									{
										Addresses: []*metadatapb.EndpointAddress{
											{
												IP:       "127.0.0.1",
												Hostname: "hostname",
											},
										},
										NotReadyAddresses: []*metadatapb.EndpointAddress{
											{
												IP:       "127.0.0.2",
												Hostname: "notready",
											},
										},
										Ports: []*metadatapb.EndpointPort{
											{
												Name:     "endpointport",
												Port:     8081,
												Protocol: metadatapb.TCP,
											},
										},
									},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "simple service",
			updates: []resourceUpdate{
				&service{
					s: &v1.Service{
						ObjectMeta: metav1.ObjectMeta{
							Name: "myservice",
						},
						Spec: v1.ServiceSpec{
							Ports: []v1.ServicePort{
								{
									Name:     "myport",
									Protocol: v1.ProtocolTCP,
									Port:     8080,
								},
							},
						},
					},
					ns: "test",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "services",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Service{
							Service: &metadatapb.Service{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "myservice",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.ServiceSpec{
									Ports: []*metadatapb.ServicePort{
										{
											Name:     "myport",
											Protocol: metadatapb.TCP,
											Port:     8080,
										},
									},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "simple replicaset",
			updates: []resourceUpdate{
				&replicaSet{
					rs: &appsv1.ReplicaSet{
						ObjectMeta: metav1.ObjectMeta{
							Name: "myrs",
						},
						Spec: appsv1.ReplicaSetSpec{
							Template: v1.PodTemplateSpec{
								ObjectMeta: metav1.ObjectMeta{
									Name: "mypod",
								},
								Spec: v1.PodSpec{
									Hostname: "hostname",
								},
							},
							Selector: &metav1.LabelSelector{
								MatchExpressions: []metav1.LabelSelectorRequirement{
									{
										Key:      "test",
										Operator: metav1.LabelSelectorOpIn,
										Values:   []string{"test2"},
									},
								},
							},
						},
					},
					ns: "test",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "replicasets",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_ReplicaSet{
							ReplicaSet: &metadatapb.ReplicaSet{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "myrs",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.ReplicaSetSpec{
									Replicas: 1,
									Template: &metadatapb.PodTemplateSpec{
										Metadata: &metadatapb.ObjectMetadata{
											Name:            "mypod",
											OwnerReferences: []*metadatapb.OwnerReference{},
										},
										Spec: &metadatapb.PodSpec{
											Hostname: "hostname",
										},
									},
									Selector: &metadatapb.LabelSelector{
										MatchExpressions: []*metadatapb.LabelSelectorRequirement{
											{
												Key:      "test",
												Operator: "In",
												Values:   []string{"test2"},
											},
										},
									},
								},
								Status: &metadatapb.ReplicaSetStatus{
									Conditions: []*metadatapb.ReplicaSetCondition{},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "recreate deployment",
			updates: []resourceUpdate{
				&deployment{
					d: &appsv1.Deployment{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mydeployment",
						},
						Spec: appsv1.DeploymentSpec{
							Template: v1.PodTemplateSpec{
								ObjectMeta: metav1.ObjectMeta{
									Name: "mypod",
								},
								Spec: v1.PodSpec{
									Hostname: "hostname",
								},
							},
							Selector: &metav1.LabelSelector{
								MatchExpressions: []metav1.LabelSelectorRequirement{
									{
										Key:      "test",
										Operator: metav1.LabelSelectorOpIn,
										Values:   []string{"test2"},
									},
								},
							},
							Strategy: appsv1.DeploymentStrategy{
								Type: appsv1.RecreateDeploymentStrategyType,
							},
							RevisionHistoryLimit:    int32ptr(1),
							ProgressDeadlineSeconds: int32ptr(1),
						},
					},
					ns: "test",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "deployments",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Deployment{
							Deployment: &metadatapb.Deployment{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mydeployment",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.DeploymentSpec{
									Replicas: 0,
									Template: &metadatapb.PodTemplateSpec{
										Metadata: &metadatapb.ObjectMetadata{
											Name:            "mypod",
											OwnerReferences: []*metadatapb.OwnerReference{},
										},
										Spec: &metadatapb.PodSpec{
											Hostname: "hostname",
										},
									},
									Selector: &metadatapb.LabelSelector{
										MatchExpressions: []*metadatapb.LabelSelectorRequirement{
											{
												Key:      "test",
												Operator: "In",
												Values:   []string{"test2"},
											},
										},
									},
									Strategy: &metadatapb.DeploymentStrategy{
										Type: metadatapb.DEPLOYMENT_STRATEGY_RECREATE,
									},
									RevisionHistoryLimit:    1,
									ProgressDeadlineSeconds: 1,
								},
								Status: &metadatapb.DeploymentStatus{},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "invalid deployment type",
			updates: []resourceUpdate{
				&deployment{
					d: &appsv1.Deployment{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mydeployment",
						},
						Spec: appsv1.DeploymentSpec{
							Template: v1.PodTemplateSpec{
								ObjectMeta: metav1.ObjectMeta{
									Name: "mypod",
								},
								Spec: v1.PodSpec{
									Hostname: "hostname",
								},
							},
							Selector: &metav1.LabelSelector{
								MatchExpressions: []metav1.LabelSelectorRequirement{
									{
										Key:      "test",
										Operator: metav1.LabelSelectorOpIn,
										Values:   []string{"test2"},
									},
								},
							},
							Strategy: appsv1.DeploymentStrategy{
								Type: appsv1.DeploymentStrategyType("invalid"),
							},
						},
					},
					ns: "test",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "deployments",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Deployment{
							Deployment: &metadatapb.Deployment{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mydeployment",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.DeploymentSpec{
									Replicas: 0,
									Template: &metadatapb.PodTemplateSpec{
										Metadata: &metadatapb.ObjectMetadata{
											Name:            "mypod",
											OwnerReferences: []*metadatapb.OwnerReference{},
										},
										Spec: &metadatapb.PodSpec{
											Hostname: "hostname",
										},
									},
									Selector: &metadatapb.LabelSelector{
										MatchExpressions: []*metadatapb.LabelSelectorRequirement{
											{
												Key:      "test",
												Operator: "In",
												Values:   []string{"test2"},
											},
										},
									},
									Strategy: &metadatapb.DeploymentStrategy{
										Type: metadatapb.DEPLOYMENT_STRATEGY_UNKNOWN,
									},
								},
								Status: &metadatapb.DeploymentStatus{},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
		{
			name: "rolling deployment",
			updates: []resourceUpdate{
				&deployment{
					d: &appsv1.Deployment{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mydeployment",
						},
						Spec: appsv1.DeploymentSpec{
							Template: v1.PodTemplateSpec{
								ObjectMeta: metav1.ObjectMeta{
									Name: "mypod",
								},
								Spec: v1.PodSpec{
									Hostname: "hostname",
								},
							},
							Selector: &metav1.LabelSelector{
								MatchExpressions: []metav1.LabelSelectorRequirement{
									{
										Key:      "test",
										Operator: metav1.LabelSelectorOpIn,
										Values:   []string{"test2"},
									},
								},
							},
							Strategy: appsv1.DeploymentStrategy{
								Type: appsv1.RollingUpdateDeploymentStrategyType,
								RollingUpdate: &appsv1.RollingUpdateDeployment{
									MaxUnavailable: intStr("0"),
									MaxSurge:       intStr("1"),
								},
							},
							RevisionHistoryLimit:    int32ptr(1),
							ProgressDeadlineSeconds: int32ptr(1),
						},
						Status: appsv1.DeploymentStatus{
							Conditions: []appsv1.DeploymentCondition{
								{
									Type: appsv1.DeploymentReplicaFailure,
								},
								{
									Type: appsv1.DeploymentConditionType("invalid"),
								},
							},
							CollisionCount: int32ptr(1),
						},
					},
					ns: "test",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "deployments",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Deployment{
							Deployment: &metadatapb.Deployment{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mydeployment",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.DeploymentSpec{
									Replicas: 0,
									Template: &metadatapb.PodTemplateSpec{
										Metadata: &metadatapb.ObjectMetadata{
											Name:            "mypod",
											OwnerReferences: []*metadatapb.OwnerReference{},
										},
										Spec: &metadatapb.PodSpec{
											Hostname: "hostname",
										},
									},
									Selector: &metadatapb.LabelSelector{
										MatchExpressions: []*metadatapb.LabelSelectorRequirement{
											{
												Key:      "test",
												Operator: "In",
												Values:   []string{"test2"},
											},
										},
									},
									Strategy: &metadatapb.DeploymentStrategy{
										Type: metadatapb.DEPLOYMENT_STRATEGY_ROLLING_UPDATE,
										RollingUpdate: &metadatapb.RollingUpdateDeployment{
											MaxUnavailable: "0",
											MaxSurge:       "1",
										},
									},
									RevisionHistoryLimit:    1,
									ProgressDeadlineSeconds: 1,
								},
								Status: &metadatapb.DeploymentStatus{
									Conditions: []*metadatapb.DeploymentCondition{
										{
											Type: metadatapb.DEPLOYMENT_CONDITION_REPLICA_FAILURE,
										},
										{
											Type: metadatapb.DEPLOYMENT_CONDITION_TYPE_UNKNOWN,
										},
									},
									CollisionCount: 1,
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			client := fake.NewSimpleClientset()

			watchStart := make(chan string)
			defer close(watchStart)

			client.PrependWatchReactor("*", func(action clienttesting.Action) (bool, watch.Interface, error) {
				gvr := action.GetResource()
				ns := action.GetNamespace()
				watch, err := client.Tracker().Watch(gvr, ns)
				if err != nil {
					return false, nil, err
				}
				watchStart <- gvr.Resource
				return true, watch, nil
			})

			updateCh := make(chan *k8smeta.K8sResourceMessage, 2*len(tc.expectedUpdates))

			for _, u := range tc.inits {
				err := u.Apply(context.Background(), client)
				require.NoError(t, err)
			}

			controller, err := k8smeta.NewControllerWithClientSet([]string{v1.NamespaceAll}, updateCh, client)
			require.NoError(t, err)
			defer controller.Stop()

			watchersStarted := map[string]bool{
				// This list needs to be kept up-to-date with the list of watchers started in k8s_metadata_controller.go
				"nodes":       false,
				"namespaces":  false,
				"pods":        false,
				"endpoints":   false,
				"services":    false,
				"replicasets": false,
				"deployments": false,
			}
			allStarted := func() bool {
				for _, started := range watchersStarted {
					if !started {
						return false
					}
				}
				return true
			}
			for resource := range watchStart {
				watchersStarted[resource] = true
				if allStarted() {
					break
				}
			}

			for _, u := range tc.updates {
				err := u.Apply(context.Background(), client)
				require.NoError(t, err)
			}

			updates := []*k8smeta.K8sResourceMessage{}
			for u := range updateCh {
				updates = append(updates, u)
				if len(updates) >= len(tc.expectedUpdates) {
					break
				}
			}

			// The fake k8s client doesn't output reasonable timestamps, so we set all timestamps to zero.
			expected := zeroTimestamps(tc.expectedUpdates)
			actual := zeroTimestamps(updates)

			assert.ElementsMatch(t, expected, actual)
		})
	}
}

func TestControllerWithNotWatchedNameSpaces(t *testing.T) {
	testCases := []struct {
		name            string
		updates         []resourceUpdate
		inits           []resourceUpdate
		expectedUpdates []*k8smeta.K8sResourceMessage
	}{
		{
			name: "simple pod",
			updates: []resourceUpdate{
				&pod{
					p: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{
							Name: "mypod",
						},
						Spec: v1.PodSpec{
							Hostname: "hostname",
						},
					},
					ns: "test",
					t:  create,
				},
				&pod{
					p: &v1.Pod{
						ObjectMeta: metav1.ObjectMeta{
							Name: "myunwatchedpod",
						},
						Spec: v1.PodSpec{
							Hostname: "hostname",
						},
					},
					ns: "dontwatchme",
					t:  create,
				},
			},
			expectedUpdates: []*k8smeta.K8sResourceMessage{
				{
					ObjectType: "pods",
					Object: &storepb.K8SResource{
						Resource: &storepb.K8SResource_Pod{
							Pod: &metadatapb.Pod{
								Metadata: &metadatapb.ObjectMetadata{
									Name:            "mypod",
									Namespace:       "test",
									OwnerReferences: []*metadatapb.OwnerReference{},
								},
								Spec: &metadatapb.PodSpec{
									Hostname: "hostname",
								},
								Status: &metadatapb.PodStatus{
									Conditions:        []*metadatapb.PodCondition{},
									ContainerStatuses: []*metadatapb.ContainerStatus{},
								},
							},
						},
					},
					EventType: watch.Added,
				},
			},
		},
	}
	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			client := fake.NewSimpleClientset()

			watchStart := make(chan string)
			defer close(watchStart)

			client.PrependWatchReactor("*", func(action clienttesting.Action) (bool, watch.Interface, error) {
				gvr := action.GetResource()
				ns := action.GetNamespace()
				watch, err := client.Tracker().Watch(gvr, ns)
				if err != nil {
					return false, nil, err
				}
				watchStart <- gvr.Resource
				return true, watch, nil
			})

			updateCh := make(chan *k8smeta.K8sResourceMessage, 2*len(tc.expectedUpdates))

			for _, u := range tc.inits {
				err := u.Apply(context.Background(), client)
				require.NoError(t, err)
			}

			namespaces := []string{"test"}
			controller, err := k8smeta.NewControllerWithClientSet(namespaces, updateCh, client)
			require.NoError(t, err)
			defer controller.Stop()

			watchersStarted := map[string]bool{
				// This list needs to be kept up-to-date with the list of watchers started in k8s_metadata_controller.go
				"nodes":       false,
				"namespaces":  false,
				"pods":        false,
				"endpoints":   false,
				"services":    false,
				"replicasets": false,
				"deployments": false,
			}
			allStarted := func() bool {
				for _, started := range watchersStarted {
					if !started {
						return false
					}
				}
				return true
			}
			for resource := range watchStart {
				watchersStarted[resource] = true
				if allStarted() {
					break
				}
			}

			for _, u := range tc.updates {
				err := u.Apply(context.Background(), client)
				require.NoError(t, err)
			}

			updates := []*k8smeta.K8sResourceMessage{}
			for u := range updateCh {
				updates = append(updates, u)
				if len(updates) >= len(tc.expectedUpdates) {
					break
				}
			}

			assert.Equal(t, 1, len(updates))
			ns := updates[0].Object.GetPod().GetMetadata().GetNamespace()
			assert.Equal(t, "test", ns)
		})
	}
}

func TestController_InClusterConfig(t *testing.T) {
	updateCh := make(chan *k8smeta.K8sResourceMessage)
	controller, err := k8smeta.NewController([]string{v1.NamespaceAll}, updateCh)
	assert.Error(t, err)
	assert.ErrorContains(t, err, "unable to load in-cluster configuration")
	assert.Nil(t, controller)
}

func zeroTimestamps(updates []*k8smeta.K8sResourceMessage) []*k8smeta.K8sResourceMessage {
	out := make([]*k8smeta.K8sResourceMessage, len(updates))
	for i, u := range updates {
		v := *u
		if v.Object.GetPod() != nil {
			v.Object.GetPod().GetMetadata().CreationTimestampNS = 0
			v.Object.GetPod().GetMetadata().DeletionTimestampNS = 0
			v.Object.GetPod().GetStatus().CreatedAt = nil
		}
		if v.Object.GetNode() != nil {
			v.Object.GetNode().GetMetadata().CreationTimestampNS = 0
			v.Object.GetNode().GetMetadata().DeletionTimestampNS = 0
		}
		if v.Object.GetNamespace() != nil {
			v.Object.GetNamespace().GetMetadata().CreationTimestampNS = 0
			v.Object.GetNamespace().GetMetadata().DeletionTimestampNS = 0
		}
		if v.Object.GetEndpoints() != nil {
			v.Object.GetEndpoints().GetMetadata().CreationTimestampNS = 0
			v.Object.GetEndpoints().GetMetadata().DeletionTimestampNS = 0
		}
		if v.Object.GetService() != nil {
			v.Object.GetService().GetMetadata().CreationTimestampNS = 0
			v.Object.GetService().GetMetadata().DeletionTimestampNS = 0
		}
		if v.Object.GetReplicaSet() != nil {
			v.Object.GetReplicaSet().GetMetadata().CreationTimestampNS = 0
			v.Object.GetReplicaSet().GetMetadata().DeletionTimestampNS = 0
			if v.Object.GetReplicaSet().GetSpec().GetTemplate() != nil {
				v.Object.GetReplicaSet().GetSpec().GetTemplate().GetMetadata().CreationTimestampNS = 0
				v.Object.GetReplicaSet().GetSpec().GetTemplate().GetMetadata().DeletionTimestampNS = 0
			}
		}
		if v.Object.GetDeployment() != nil {
			v.Object.GetDeployment().GetMetadata().CreationTimestampNS = 0
			v.Object.GetDeployment().GetMetadata().DeletionTimestampNS = 0
			if v.Object.GetDeployment().GetSpec().GetTemplate() != nil {
				v.Object.GetDeployment().GetSpec().GetTemplate().GetMetadata().CreationTimestampNS = 0
				v.Object.GetDeployment().GetSpec().GetTemplate().GetMetadata().DeletionTimestampNS = 0
			}

			for _, cond := range v.Object.GetDeployment().GetStatus().Conditions {
				cond.LastUpdateTimeNS = 0
				cond.LastTransitionTimeNS = 0
			}
		}
		out[i] = &v
	}
	return out
}

type resourceUpdate interface {
	Apply(context.Context, kubernetes.Interface) error
}

type resourceUpdateType string

const (
	create resourceUpdateType = "create"
	update resourceUpdateType = "update"
	del    resourceUpdateType = "delete"
)

type pod struct {
	p  *v1.Pod
	ns string
	t  resourceUpdateType
}

func (p *pod) Apply(ctx context.Context, c kubernetes.Interface) error {
	pi := c.CoreV1().Pods(p.ns)
	switch p.t {
	case create:
		_, err := pi.Create(ctx, p.p, metav1.CreateOptions{})
		return err
	case update:
		_, err := pi.Update(ctx, p.p, metav1.UpdateOptions{})
		return err
	case del:
		return pi.Delete(ctx, p.p.Name, metav1.DeleteOptions{})
	default:
		return errors.New("invalid resourceUpdateType")
	}
}

type node struct {
	n *v1.Node
	t resourceUpdateType
}

func (n *node) Apply(ctx context.Context, c kubernetes.Interface) error {
	ni := c.CoreV1().Nodes()
	switch n.t {
	case create:
		_, err := ni.Create(ctx, n.n, metav1.CreateOptions{})
		return err
	case update:
		_, err := ni.Update(ctx, n.n, metav1.UpdateOptions{})
		return err
	case del:
		return ni.Delete(ctx, n.n.Name, metav1.DeleteOptions{})
	default:
		return errors.New("invalid resourceUpdateType")
	}
}

type namespace struct {
	n *v1.Namespace
	t resourceUpdateType
}

func (n *namespace) Apply(ctx context.Context, c kubernetes.Interface) error {
	ni := c.CoreV1().Namespaces()
	switch n.t {
	case create:
		_, err := ni.Create(ctx, n.n, metav1.CreateOptions{})
		return err
	case update:
		_, err := ni.Update(ctx, n.n, metav1.UpdateOptions{})
		return err
	case del:
		return ni.Delete(ctx, n.n.Name, metav1.DeleteOptions{})
	default:
		return errors.New("invalid resourceUpdateType")
	}
}

type endpoints struct {
	e  *v1.Endpoints
	ns string
	t  resourceUpdateType
}

func (e *endpoints) Apply(ctx context.Context, c kubernetes.Interface) error {
	ei := c.CoreV1().Endpoints(e.ns)
	switch e.t {
	case create:
		_, err := ei.Create(ctx, e.e, metav1.CreateOptions{})
		return err
	case update:
		_, err := ei.Update(ctx, e.e, metav1.UpdateOptions{})
		return err
	case del:
		return ei.Delete(ctx, e.e.Name, metav1.DeleteOptions{})
	default:
		return errors.New("invalid resourceUpdateType")
	}
}

type service struct {
	s  *v1.Service
	ns string
	t  resourceUpdateType
}

func (s *service) Apply(ctx context.Context, c kubernetes.Interface) error {
	ei := c.CoreV1().Services(s.ns)
	switch s.t {
	case create:
		_, err := ei.Create(ctx, s.s, metav1.CreateOptions{})
		return err
	case update:
		_, err := ei.Update(ctx, s.s, metav1.UpdateOptions{})
		return err
	case del:
		return ei.Delete(ctx, s.s.Name, metav1.DeleteOptions{})
	default:
		return errors.New("invalid resourceUpdateType")
	}
}

type replicaSet struct {
	rs *appsv1.ReplicaSet
	ns string
	t  resourceUpdateType
}

func (rs *replicaSet) Apply(ctx context.Context, c kubernetes.Interface) error {
	ei := c.AppsV1().ReplicaSets(rs.ns)
	switch rs.t {
	case create:
		_, err := ei.Create(ctx, rs.rs, metav1.CreateOptions{})
		return err
	case update:
		_, err := ei.Update(ctx, rs.rs, metav1.UpdateOptions{})
		return err
	case del:
		return ei.Delete(ctx, rs.rs.Name, metav1.DeleteOptions{})
	default:
		return errors.New("invalid resourceUpdateType")
	}
}

type deployment struct {
	d  *appsv1.Deployment
	ns string
	t  resourceUpdateType
}

func (d *deployment) Apply(ctx context.Context, c kubernetes.Interface) error {
	ei := c.AppsV1().Deployments(d.ns)
	switch d.t {
	case create:
		_, err := ei.Create(ctx, d.d, metav1.CreateOptions{})
		return err
	case update:
		_, err := ei.Update(ctx, d.d, metav1.UpdateOptions{})
		return err
	case del:
		return ei.Delete(ctx, d.d.Name, metav1.DeleteOptions{})
	default:
		return errors.New("invalid resourceUpdateType")
	}
}
