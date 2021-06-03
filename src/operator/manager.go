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

package main

import (
	"flag"
	"os"

	log "github.com/sirupsen/logrus"
	"k8s.io/apimachinery/pkg/runtime"
	clientgoscheme "k8s.io/client-go/kubernetes/scheme"
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
	ctrl "sigs.k8s.io/controller-runtime"

	pixiev1alpha1 "px.dev/pixie/src/operator/api/v1alpha1"
	"px.dev/pixie/src/operator/controllers"
	"px.dev/pixie/src/utils/shared/k8s"
	// +kubebuilder:scaffold:imports
)

var (
	scheme = runtime.NewScheme()
)

const (
	leaderElectionID = "27ad4010.px.dev"
)

func init() {
	_ = clientgoscheme.AddToScheme(scheme)

	_ = pixiev1alpha1.AddToScheme(scheme)
	// +kubebuilder:scaffold:scheme
}

func main() {
	var metricsAddr string
	var enableLeaderElection bool
	flag.StringVar(&metricsAddr, "metrics-addr", ":8080", "The address the metric endpoint binds to.")
	flag.BoolVar(&enableLeaderElection, "enable-leader-election", false,
		"Enable leader election for controller manager. "+
			"Enabling this will ensure there is only one active controller manager.")
	flag.Parse()

	mgr, err := ctrl.NewManager(ctrl.GetConfigOrDie(), ctrl.Options{
		Scheme:             scheme,
		MetricsBindAddress: metricsAddr,
		Port:               9443,
		LeaderElection:     enableLeaderElection,
		LeaderElectionID:   leaderElectionID,
	})
	if err != nil {
		log.WithError(err).Error("Unable to start manager")
		os.Exit(1)
	}

	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		log.WithError(err).Error("Unable to get incluster kubeconfig")
		os.Exit(1)
	}
	clientset := k8s.GetClientset(kubeConfig)

	if err = (&controllers.VizierReconciler{
		Client:     mgr.GetClient(),
		Scheme:     mgr.GetScheme(),
		Clientset:  clientset,
		RestConfig: kubeConfig,
	}).SetupWithManager(mgr); err != nil {
		log.WithError(err).Error("Unable to create controller")
		os.Exit(1)
	}
	// +kubebuilder:scaffold:builder

	log.Info("Starting manager")
	if err := mgr.Start(ctrl.SetupSignalHandler()); err != nil {
		log.WithError(err).Error("Problem running manager")
		os.Exit(1)
	}
}
