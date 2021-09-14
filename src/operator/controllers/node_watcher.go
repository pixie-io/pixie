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
	"context"
	"time"

	"github.com/blang/semver"
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	apiwatch "k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"
	"k8s.io/client-go/tools/watch"

	"px.dev/pixie/src/shared/status"
)

const (
	// If 25% of the kernel versions are incompatible, then consider Vizier in
	// a degraded state.
	degradedThreshold = .25
)

var (
	kernelMinVersion = semver.Version{Major: 4, Minor: 14, Patch: 0}
)

func nodeIsCompatible(node *v1.Node) bool {
	currentSemVer, err := semver.Make(node.Status.NodeInfo.KernelVersion)
	if err != nil {
		log.WithError(err).Error("Failed to parse current Node Kernel version")
		return true
	}
	return currentSemVer.GE(kernelMinVersion)
}

type nodeCompatTracker struct {
	incompatibleCount float64
	nodeCompatible    map[string]bool
}

func (n *nodeCompatTracker) addNode(node *v1.Node) {
	com := nodeIsCompatible(node)
	if _, ok := n.nodeCompatible[node.Name]; ok {
		n.updateNode(node)
		return
	}
	n.nodeCompatible[node.Name] = com
	if !com {
		n.incompatibleCount++
	}
}

func (n *nodeCompatTracker) updateNode(node *v1.Node) {
	oldCom, ok := n.nodeCompatible[node.Name]
	if !ok {
		n.addNode(node)
		return
	}
	com := nodeIsCompatible(node)
	if com == oldCom {
		return
	}
	n.nodeCompatible[node.Name] = com
	if com {
		n.incompatibleCount--
	} else {
		n.incompatibleCount++
	}
}

func (n *nodeCompatTracker) removeNode(node *v1.Node) {
	com, ok := n.nodeCompatible[node.Name]
	if !ok {
		return
	}
	delete(n.nodeCompatible, node.Name)
	if !com {
		n.incompatibleCount--
	}
}

func (n *nodeCompatTracker) state() *vizierState {
	if n.incompatibleCount > degradedThreshold*float64(len(n.nodeCompatible)) {
		return &vizierState{Reason: status.KernelVersionsIncompatible}
	}
	return okState()
}

// NodeWatcher is responsible for tracking the nodes from the K8s API and using the NodeInfo to determine
// whether or not Pixie can successfully collect data on the cluster.
type nodeWatcher struct {
	clientset kubernetes.Interface

	compatTracker nodeCompatTracker
	lastRV        string

	state chan<- *vizierState
}

func (nw *nodeWatcher) start(ctx context.Context) {
	nw.compatTracker = nodeCompatTracker{
		incompatibleCount: 0.0,
		nodeCompatible:    make(map[string]bool),
	}
	nodeList, err := nw.clientset.CoreV1().Nodes().List(ctx, metav1.ListOptions{})
	if err != nil {
		log.WithError(err).Fatal("Could not list nodes")
	}
	nw.lastRV = nodeList.ResourceVersion

	for i := range nodeList.Items {
		nw.compatTracker.addNode(&nodeList.Items[i])
	}
	nw.state <- nw.compatTracker.state()

	// Start watcher.
	nw.watchNodes(ctx)
}

func (nw *nodeWatcher) watchNodes(ctx context.Context) {
	for {
		watcher := cache.NewListWatchFromClient(nw.clientset.CoreV1().RESTClient(), "nodes", metav1.NamespaceAll, fields.Everything())
		retryWatcher, err := watch.NewRetryWatcher(nw.lastRV, watcher)
		if err != nil {
			log.WithError(err).Fatal("Could not start watcher for nodes")
		}

		resCh := retryWatcher.ResultChan()
		loop := true
		for loop {
			select {
			case <-ctx.Done():
				log.Info("Received cancel, stopping K8s watcher")
				return
			case c, ok := <-resCh:
				if !ok {
					loop = false
					break
				}
				s, ok := c.Object.(*metav1.Status)
				if ok && s.Status == metav1.StatusFailure {
					log.WithField("status", s.Status).WithField("msg", s.Message).WithField("reason", s.Reason).WithField("details", s.Details).Info("Received failure status in node watcher")
					// Try to start up another watcher instance.
					// Sleep a second before retrying, so as not to drive up the CPU.
					time.Sleep(1 * time.Second)
					loop = false
					break
				}

				node, ok := c.Object.(*v1.Node)
				if !ok {
					continue
				}

				// Update the lastRV, so that if the watcher restarts, it starts at the correct resource version.
				nw.lastRV = node.ObjectMeta.ResourceVersion

				switch c.Type {
				case apiwatch.Added:
					nw.compatTracker.addNode(node)
				case apiwatch.Modified:
					nw.compatTracker.updateNode(node)
				case apiwatch.Deleted:
					nw.compatTracker.removeNode(node)
				}

				nw.state <- nw.compatTracker.state()
			}
		}
	}
}
