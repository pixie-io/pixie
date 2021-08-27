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

package k8s

import (
	"context"

	v1 "k8s.io/api/core/v1"
	storagev1 "k8s.io/api/storage/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"
)

// WatchK8sResource returns a k8s watcher for the specified resource.
func WatchK8sResource(clientset kubernetes.Interface, resource string, namespace string) (watch.Interface, error) {
	watcher := cache.NewListWatchFromClient(clientset.CoreV1().RESTClient(), resource, namespace, fields.Everything())
	opts := metav1.ListOptions{}
	watch, err := watcher.Watch(opts)
	if err != nil {
		return nil, err
	}
	return watch, nil
}

// ListNodes lists the nodes in this Kubernetes cluster.
func ListNodes(clientset kubernetes.Interface) (*v1.NodeList, error) {
	opts := metav1.ListOptions{}
	return clientset.CoreV1().Nodes().List(context.Background(), opts)
}

// ListStorageClasses lists the storage classes in this Kubernetes cluster.
func ListStorageClasses(clientset kubernetes.Interface) (*storagev1.StorageClassList, error) {
	opts := metav1.ListOptions{}
	return clientset.StorageV1().StorageClasses().List(context.Background(), opts)
}
