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

	log "github.com/sirupsen/logrus"
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
	watchers []watcher
}

// watcher watches a k8s resource type and forwards the updates to the given update channel.
type watcher interface {
	StartWatcher(chan struct{})
	InitWatcher() error
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
	return NewControllerWithClientSet(updateCh, clientset)
}

// NewControllerWithClientSet creates a new Controller using the given Clientset.
func NewControllerWithClientSet(updateCh chan *K8sResourceMessage, clientset kubernetes.Interface) (*Controller, error) {
	quitCh := make(chan struct{})

	// Create a watcher for each resource.
	// The resource types we watch the K8s API for. These types are in a specific order:
	// for example, nodes and namespaces must be synced before pods, since nodes/namespaces
	// contain pods.
	watchers := []watcher{
		nodeWatcher("nodes", namespaces, updateCh, clientset),
		namespaceWatcher("namespaces", namespaces, updateCh, clientset),
		podWatcher("pods", namespaces, updateCh, clientset),
		endpointsWatcher("endpoints", namespaces, updateCh, clientset),
		serviceWatcher("services", namespaces, updateCh, clientset),
		replicaSetWatcher("replicasets", namespaces, updateCh, clientset),
		deploymentWatcher("deployments", namespaces, updateCh, clientset),
	}

	mc := &Controller{quitCh: quitCh, updateCh: updateCh, watchers: watchers}

	go func() {
		for _, w := range mc.watchers {
			err := w.InitWatcher()
			if err != nil {
				// Still return the informer because the rest of the system can recover from this.
				log.WithError(err).Error("Failed to run watcher init")
			}
			go w.StartWatcher(quitCh)
		}
	}()

	return mc, nil
}

// Stop stops all K8s watchers.
func (mc *Controller) Stop() {
	mc.once.Do(func() {
		close(mc.quitCh)
	})
}
