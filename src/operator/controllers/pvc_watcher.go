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
	// The name of the metadata-pvc.
	metadataPVC = "metadata-pv-claim"
)

// Returns whether the storage class name requested by the pvc is valid for the Kubernetes instance.
func metadataPVCState(clientset kubernetes.Interface, pvc *v1.PersistentVolumeClaim) *vizierState {
	if pvc == nil {
		return &vizierState{Reason: status.MetadataPVCMissing}
	}

	if pvc.Status.Phase == v1.ClaimBound {
		return okState()
	}

	_, err := clientset.StorageV1().StorageClasses().Get(context.Background(), *pvc.Spec.StorageClassName, metav1.GetOptions{})
	if err != nil {
		return &vizierState{Reason: status.MetadataPVCStorageClassUnavailable}
	}
	return &vizierState{Reason: status.MetadataPVCPendingBinding}
}

// pvcWatcher is responsible for tracking the PVC from the K8s API and using the PVC Info to determine
// whether or not Pixie can successfully run the metadata service on this cluster.
type pvcWatcher struct {
	clientset kubernetes.Interface
	lastRV    string

	namespace string

	state chan<- *vizierState
}

func (pw *pvcWatcher) start(ctx context.Context) {
	pvcs, err := pw.clientset.CoreV1().PersistentVolumeClaims(pw.namespace).List(ctx, metav1.ListOptions{})
	if err != nil {
		log.WithError(err).Fatal("Could not list pvcs")
	}
	pw.lastRV = pvcs.ResourceVersion

	var pvc *v1.PersistentVolumeClaim
	for i := range pvcs.Items {
		if pvcs.Items[i].Name == metadataPVC {
			pvc = &pvcs.Items[i]
			break
		}
	}
	pw.state <- metadataPVCState(pw.clientset, pvc)

	// Start watcher.
	pw.watchPVCs(ctx)
}

func (pw *pvcWatcher) watchPVCs(ctx context.Context) {
	for {
		watcher := cache.NewListWatchFromClient(pw.clientset.CoreV1().RESTClient(), "persistentvolumeclaims", pw.namespace, fields.Everything())
		retryWatcher, err := watch.NewRetryWatcher(pw.lastRV, watcher)
		if err != nil {
			log.WithError(err).Fatal("Could not start watcher for PVCs")
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
					log.WithField("status", s.Status).WithField("msg", s.Message).WithField("reason", s.Reason).WithField("details", s.Details).Info("Received failure status in PVC watcher")
					// Try to start up another watcher instance.
					// Sleep a second before retrying, so as not to drive up the CPU.
					time.Sleep(1 * time.Second)
					loop = false
					break
				}

				pvc, ok := c.Object.(*v1.PersistentVolumeClaim)
				if !ok {
					continue
				}

				// Update the lastRV, so that if the watcher restarts, it starts at the correct resource version.
				pw.lastRV = pvc.ObjectMeta.ResourceVersion
				if pvc.Name != metadataPVC {
					continue
				}

				switch c.Type {
				case apiwatch.Added, apiwatch.Modified:
					pw.state <- metadataPVCState(pw.clientset, pvc)
				case apiwatch.Deleted:
					pw.state <- metadataPVCState(pw.clientset, nil)
				}
			}
		}
	}
}
