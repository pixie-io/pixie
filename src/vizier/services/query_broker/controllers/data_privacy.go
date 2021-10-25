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

	log "github.com/sirupsen/logrus"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/rest"

	pixie "px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/operator/client/versioned"
)

// getVizierCRD gets the Vizier CRD for the running Vizier, if running using an operator.
func getVizierCRD(ns string) (*pixie.Vizier, error) {
	// There is a specific config for services running in the cluster.
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		return nil, err
	}

	// Create k8s client.
	vzCrdClient, err := versioned.NewForConfig(kubeConfig)
	if err != nil {
		log.WithError(err).Error("Failed to initialize vizier CRD client")
		return nil, err
	}

	viziers, err := vzCrdClient.PxV1alpha1().Viziers(ns).List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return nil, err
	}
	if len(viziers.Items) > 0 {
		return &viziers.Items[0], nil
	}
	return nil, errors.New("No vizier CRD found")
}

type vizierCachedDataPrivacy struct {
	dataAccess pixie.DataAccessLevel
}

// ShouldRedactSensitiveColumns returns whether we should redact sensitive columns or not.
func (dp *vizierCachedDataPrivacy) ShouldRedactSensitiveColumns(ctx context.Context) (bool, error) {
	return dp.dataAccess == pixie.DataAccessRestricted, nil
}

// CreateDataPrivacyManager creates a privacy manager for the namespace.
func CreateDataPrivacyManager(ns string) (DataPrivacy, error) {
	// We only get this value once.
	vz, err := getVizierCRD(ns)
	if err != nil {
		return &vizierCachedDataPrivacy{
			dataAccess: pixie.DataAccessFull,
		}, nil
	}
	return &vizierCachedDataPrivacy{
		dataAccess: vz.Spec.DataAccess,
	}, nil
}
