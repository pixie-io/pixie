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

	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	k8serrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"

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
	if err != nil && k8serrors.IsNotFound(err) { // If no storage class is available, we cannot dynamically provision the PV.
		return &vizierState{Reason: status.MetadataPVCStorageClassUnavailable}
	} else if err != nil {
		log.WithError(err).Error("Failed to get storageclasses")
	}
	return &vizierState{Reason: status.MetadataPVCPendingBinding}
}

// pvcWatcher is responsible for tracking the PVC from the K8s API and using the PVC Info to determine
// whether or not Pixie can successfully run the metadata service on this cluster.
type pvcWatcher struct {
	clientset kubernetes.Interface
	factory   informers.SharedInformerFactory

	namespace string

	state chan<- *vizierState
}

func (pw *pvcWatcher) start(ctx context.Context) {
	informer := pw.factory.Core().V1().PersistentVolumeClaims().Informer()
	stopper := make(chan struct{})
	defer close(stopper)
	_, _ = informer.AddEventHandler(cache.ResourceEventHandlerFuncs{
		AddFunc:    pw.onAdd,
		UpdateFunc: pw.onUpdate,
		DeleteFunc: pw.onDelete,
	})
	informer.Run(stopper)
}

func (pw *pvcWatcher) isMetadataPVC(obj interface{}) bool {
	pvc, ok := obj.(*v1.PersistentVolumeClaim)
	if !ok {
		return false
	}
	return pvc.Namespace == pw.namespace && pvc.Name == metadataPVC
}

func (pw *pvcWatcher) onAdd(obj interface{}) {
	if !pw.isMetadataPVC(obj) {
		return
	}
	pw.state <- metadataPVCState(pw.clientset, obj.(*v1.PersistentVolumeClaim))
}

func (pw *pvcWatcher) onUpdate(oldObj, newObj interface{}) {
	if !pw.isMetadataPVC(newObj) {
		return
	}
	pw.state <- metadataPVCState(pw.clientset, newObj.(*v1.PersistentVolumeClaim))
}

func (pw *pvcWatcher) onDelete(obj interface{}) {
	if !pw.isMetadataPVC(obj) {
		return
	}
	pw.state <- metadataPVCState(pw.clientset, nil)
}
