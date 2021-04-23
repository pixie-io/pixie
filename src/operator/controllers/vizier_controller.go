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
	"k8s.io/apimachinery/pkg/runtime"
	ctrl "sigs.k8s.io/controller-runtime"
	"sigs.k8s.io/controller-runtime/pkg/client"

	pixiev1alpha1 "px.dev/pixie/src/operator/api/v1alpha1"
)

// VizierReconciler reconciles a Vizier object
type VizierReconciler struct {
	client.Client
	Scheme *runtime.Scheme
}

// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers,verbs=get;list;watch;create;update;patch;delete
// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers/status,verbs=get;update;patch

// Reconcile updates the Vizier running in the cluster to match the expected state.
func (r *VizierReconciler) Reconcile(ctx context.Context, req ctrl.Request) (ctrl.Result, error) {
	log.Info("Reconciling...")
	// your logic here

	return ctrl.Result{}, nil
}

// SetupWithManager sets up the reconciler.
func (r *VizierReconciler) SetupWithManager(mgr ctrl.Manager) error {
	return ctrl.NewControllerManagedBy(mgr).
		For(&pixiev1alpha1.Vizier{}).
		Complete(r)
}
