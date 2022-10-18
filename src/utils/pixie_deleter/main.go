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
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/utils/shared/k8s"
)

func init() {
	pflag.String("namespace", "pl", "The namespace this job runs in.")
	pflag.String("vizier_name", "pixie", "The name of the vizier CRD.")
}

func main() {
	pflag.Parse()

	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	ns := viper.GetString("namespace")
	vzName := viper.GetString("vizier_name")

	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		log.WithError(err).Fatal("Unable to get incluster kubeconfig")
	}
	clientset := k8s.GetClientset(kubeConfig)

	od := k8s.ObjectDeleter{
		Namespace:  ns,
		Clientset:  clientset,
		RestConfig: kubeConfig,
		Timeout:    2 * time.Minute,
	}

	// First delete non-bootstrap items. For example, avoid clusterrole objects as these may prevent following deletes from going through.
	vzNameLabelSelector := metav1.FormatLabelSelector(&metav1.LabelSelector{
		MatchExpressions: []metav1.LabelSelectorRequirement{
			{
				Key:      "vizier-name",
				Operator: metav1.LabelSelectorOpIn,
				Values:   []string{vzName},
			},
			{
				Key:      "vizier-bootstrap",
				Operator: metav1.LabelSelectorOpNotIn,
				Values:   []string{"true"},
			},
		},
	})

	_, _ = od.DeleteByLabel(vzNameLabelSelector)

	// Delete clusterrole objects.
	labelSelector := metav1.FormatLabelSelector(&metav1.LabelSelector{
		MatchExpressions: []metav1.LabelSelectorRequirement{
			{
				Key:      "vizier-name",
				Operator: metav1.LabelSelectorOpIn,
				Values:   []string{vzName},
			},
		},
	})

	_, _ = od.DeleteByLabel(labelSelector)
}
