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

package k8smeta

import (
	"sync"
	"time"

	"k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
)

const (
	// resourceUpdateTTL is how long the k8s update live in the DataStore.
	resourceUpdateTTL = 24 * time.Hour
)

// Controller listens to any metadata updates from the K8s API and forwards them
// to a channel where it can be processed.
type Controller struct {
	quitCh   chan struct{}
	updateCh chan *K8sResourceMessage
	once     sync.Once
}

// NewController creates a new Controller.
func NewController(namespaces []string, updateCh chan *K8sResourceMessage) (*Controller, error) {
	// There is a specific config for services running in the cluster.
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		return nil, err
	}

	// Create k8s client.
	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		return nil, err
	}
	return NewControllerWithClientSet(namespaces, updateCh, clientset)
}

// NewControllerWithClientSet creates a new Controller using the given Clientset.
func NewControllerWithClientSet(namespaces []string, updateCh chan *K8sResourceMessage, clientset kubernetes.Interface) (*Controller, error) {
	mc := &Controller{quitCh: make(chan struct{}), updateCh: updateCh}
	go mc.startWithClientSet(namespaces, clientset)

	return mc, nil
}

// startWithClientSet starts the controller
func (mc *Controller) startWithClientSet(namespaces []string, clientset kubernetes.Interface) {
	factory := informers.NewSharedInformerFactory(clientset, 12*time.Hour)

	// Create a watcher for each resource.
	// The resource types we watch the K8s API for.
	startNodeWatcher(mc.updateCh, mc.quitCh, factory)
	startNamespaceWatcher(mc.updateCh, mc.quitCh, factory)

	var namespacedFactories []informers.SharedInformerFactory
	for _, ns := range namespaces {
		factory := informers.NewSharedInformerFactoryWithOptions(clientset, 12*time.Hour, informers.WithNamespace(ns))
		namespacedFactories = append(namespacedFactories, factory)
	}

	startPodWatcher(mc.updateCh, mc.quitCh, namespacedFactories)
	startEndpointsWatcher(mc.updateCh, mc.quitCh, namespacedFactories)
	startServiceWatcher(mc.updateCh, mc.quitCh, namespacedFactories)
	startReplicaSetWatcher(mc.updateCh, mc.quitCh, namespacedFactories)
	startDeploymentWatcher(mc.updateCh, mc.quitCh, namespacedFactories)
}

// Stop stops all K8s watchers.
func (mc *Controller) Stop() {
	mc.once.Do(func() {
		close(mc.quitCh)
	})
}
