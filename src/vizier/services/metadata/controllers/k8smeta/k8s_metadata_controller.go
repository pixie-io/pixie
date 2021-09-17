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
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"

	"px.dev/pixie/src/vizier/services/metadata/storepb"
)

const (
	// refreshUpdateDuration is how frequently we request all k8s updates to
	// refresh the ones in our DataStore.
	refreshUpdateDuration = 12 * time.Hour
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
	Sync(storedUpdates []*storepb.K8SResource) error
	StartWatcher(chan struct{}, *sync.WaitGroup)
}

func listObject(resource string, clientset *kubernetes.Clientset) (runtime.Object, error) {
	watcher := cache.NewListWatchFromClient(clientset.CoreV1().RESTClient(), resource, v1.NamespaceAll, fields.Everything())
	opts := metav1.ListOptions{}
	return watcher.List(opts)
}

// NewController creates a new Controller.
func NewController(mds Store, updateCh chan *K8sResourceMessage) (*Controller, error) {
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

	quitCh := make(chan struct{})

	// Create a watcher for each resource.
	// The resource types we watch the K8s API for. These types are in a specific order:
	// for example, nodes and namespaces must be synced before pods, since nodes/namespaces
	// contain pods.
	watchers := []watcher{
		NewNodeWatcher("nodes", updateCh, clientset),
		NewNamespaceWatcher("namespaces", updateCh, clientset),
		NewPodWatcher("pods", updateCh, clientset),
		NewEndpointsWatcher("endpoints", updateCh, clientset),
		NewServiceWatcher("services", updateCh, clientset),
	}

	mc := &Controller{quitCh: quitCh, updateCh: updateCh, watchers: watchers}

	go mc.Start(mds)

	return mc, nil
}

// Start starts the k8s watcher. Every 12h, it will resync such that the updates from the
// last 24h will always contain updates from currently running resources.
func (mc *Controller) Start(mds Store) {
	// Loop the sync/watch so that if it ever fails because of an intermittent error, it
	// will continue trying to collect data.
	for {
		select {
		case <-mc.quitCh:
			return // Quit signaled, so exit the goroutine.
		default:
			err := mc.runSyncWatchLoop(mds)
			if err == nil {
				return // Quit signaled, so exit the goroutine.
			}
			log.WithError(err).Info("Failed K8s metadata sync/watch... Restarting.")
			// Wait 5 minutes before retrying, however if stop is called, just return.
			select {
			case <-mc.quitCh:
				return
			case <-time.After(5 * time.Minute):
				continue
			}
		}
	}
}

func (mc *Controller) runSyncWatchLoop(mds Store) error {
	// Run initial sync and watch.
	watcherQuitCh := make(chan struct{})
	var wg sync.WaitGroup

	err := mc.syncAndWatch(mds, watcherQuitCh, &wg)
	if err != nil {
		return err
	}

	ticker := time.NewTicker(refreshUpdateDuration)

	defer func() {
		close(watcherQuitCh)
		ticker.Stop()
	}()

	for {
		select {
		case <-ticker.C:
			close(watcherQuitCh) // Stop previous watcher, resync to send updates for all currently running resources.
			watcherQuitCh = make(chan struct{})
			wg.Wait()
			err = mc.syncAndWatch(mds, watcherQuitCh, &wg)
			if err != nil {
				return err
			}
		case <-mc.quitCh:
			return nil
		}
	}
}

// Start syncs the state stored in the datastore with what is currently running in k8s.
func (mc *Controller) syncAndWatch(mds Store, quitCh chan struct{}, wg *sync.WaitGroup) error {
	lastUpdate, err := mds.GetUpdateVersion(KelvinUpdateTopic)
	if err != nil {
		log.WithError(err).Info("Failed to get latest update version")
		return err
	}
	storedUpdates, err := mds.FetchFullResourceUpdates(0, lastUpdate)
	if err != nil {
		log.WithError(err).Info("Failed to fetch resource updates")
		return err
	}

	for _, w := range mc.watchers {
		err := w.Sync(storedUpdates)
		if err != nil {
			log.WithError(err).Info("Failed to sync metadata")
			return err
		}
		wg.Add(1)
		go w.StartWatcher(quitCh, wg)
	}

	return nil
}

// Stop stops all K8s watchers.
func (mc *Controller) Stop() {
	mc.once.Do(func() {
		close(mc.quitCh)
	})
}
