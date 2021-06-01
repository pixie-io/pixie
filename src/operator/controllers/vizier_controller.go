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
	"errors"
	"strings"

	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	"k8s.io/apimachinery/pkg/runtime"
	ctrl "sigs.k8s.io/controller-runtime"
	"sigs.k8s.io/controller-runtime/pkg/client"

	"px.dev/pixie/src/api/proto/cloudpb"
	pixiev1alpha1 "px.dev/pixie/src/operator/api/v1alpha1"
	"px.dev/pixie/src/shared/services"
)

// VizierReconciler reconciles a Vizier object
type VizierReconciler struct {
	client.Client
	Scheme *runtime.Scheme
}

// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers,verbs=get;list;watch;create;update;patch;delete
// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers/status,verbs=get;update;patch

func getCloudClientConnection(cloudAddr string) (*grpc.ClientConn, error) {
	isInternal := strings.ContainsAny(cloudAddr, "cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}

	c, err := grpc.Dial(cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return c, nil
}

func getLatestVizierVersion(ctx context.Context, conn *grpc.ClientConn) (string, error) {
	client := cloudpb.NewArtifactTrackerClient(conn)

	req := &cloudpb.GetArtifactListRequest{
		ArtifactName: "vizier",
		ArtifactType: cloudpb.AT_CONTAINER_SET_YAMLS,
		Limit:        1,
	}
	resp, err := client.GetArtifactList(ctx, req)
	if err != nil {
		return "", err
	}

	if len(resp.Artifact) != 1 {
		return "", errors.New("Could not find Vizier artifact")
	}

	return resp.Artifact[0].VersionStr, nil
}

// Reconcile updates the Vizier running in the cluster to match the expected state.
func (r *VizierReconciler) Reconcile(ctx context.Context, req ctrl.Request) (ctrl.Result, error) {
	log.WithField("req", req).Info("Reconciling...")

	// Fetch vizier CRD to determine what operation should be performed.
	var vizier pixiev1alpha1.Vizier
	if err := r.Get(ctx, req.NamespacedName, &vizier); err != nil {
		// Vizier CRD deleted. The vizier instance should also be deleted.
		return ctrl.Result{}, r.deleteVizier(ctx, req)
	}

	if vizier.Status.VizierPhase == pixiev1alpha1.VizierPhaseNone {
		// We are creating a new vizier instance.
		return ctrl.Result{}, r.createVizier(ctx, req, &vizier)
	}

	// Vizier CRD has been updated, and we should update the running vizier accordingly.
	return ctrl.Result{}, r.updateVizier(ctx, req, &vizier)
}

// updateVizier updates the vizier instance according to the spec. As of the current moment, we only support updates to the Vizier version.
// Other updates to the Vizier spec will be ignored.
func (r *VizierReconciler) updateVizier(ctx context.Context, req ctrl.Request, vz *pixiev1alpha1.Vizier) error {
	return errors.New("Not yet implemented")
}

// deleteVizier deletes the vizier instance in the given namespace.
func (r *VizierReconciler) deleteVizier(ctx context.Context, req ctrl.Request) error {
	return errors.New("Not yet implemented")
}

// createVizier deploys a new vizier instance in the given namespace.
func (r *VizierReconciler) createVizier(ctx context.Context, req ctrl.Request, vz *pixiev1alpha1.Vizier) error {
	cloudClient, err := getCloudClientConnection(vz.Spec.CloudAddr)
	if err != nil {
		return err
	}

	// If no version is set, we should fetch the latest version. This will trigger another reconcile that will do
	// the actual vizier deployment.
	if vz.Spec.Version == "" {
		latest, err := getLatestVizierVersion(ctx, cloudClient)
		if err != nil {
			return err
		}
		vz.Spec.Version = latest
		err = r.Update(ctx, vz)
		if err != nil {
			return err
		}
		return nil
	}

	// TODO(michellenguyen): We should pull our cert-provisioner job into here. Eventually, we can make this a goroutine
	// which checks when certs are about to expire.

	err = r.deployVizierDeps(ctx, req, vz)
	if err != nil {
		return err
	}

	err = r.deployVizier(ctx, req, vz)
	if err != nil {
		return err
	}

	return nil
}

// deployVizierDeps deploys the vizier deps to the given namespace. This includes generating certs
// along with deploying deps like etcd and nats.
func (r *VizierReconciler) deployVizierDeps(ctx context.Context, req ctrl.Request, vz *pixiev1alpha1.Vizier) error {
	return errors.New("Not yet implemented")
}

// deployVizier deploys the core pods and services for running vizier.
func (r *VizierReconciler) deployVizier(ctx context.Context, req ctrl.Request, vz *pixiev1alpha1.Vizier) error {
	return errors.New("Not yet implemented")
}

// SetupWithManager sets up the reconciler.
func (r *VizierReconciler) SetupWithManager(mgr ctrl.Manager) error {
	return ctrl.NewControllerManagedBy(mgr).
		For(&pixiev1alpha1.Vizier{}).
		Complete(r)
}
