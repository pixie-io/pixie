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

package controllers_test

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/cloud/api/controllers"
)

func TestPrettifyClusterName(t *testing.T) {
	tests := []struct {
		Name              string
		ClusterName       string
		PrettyClusterName string
		Expanded          bool
	}{
		{
			"basic GKE",
			"gke_pl-dev-infra_us-west1-a_jenkins-test-cluster_1",
			"gke:jenkins-test-cluster_1",
			false,
		},
		{
			"expanded GKE",
			"gke_pl-dev-infra_us-west1-a_jenkins-test-cluster_1",
			"gke:jenkins-test-cluster_1 (pl-dev-infra)",
			true,
		},
		{
			"basic eks",
			"arn:aws:eks:us-east-2:016013129672:cluster/skylab4-my-org",
			"eks:skylab4-my-org",
			false,
		},
		{
			"basic aks",
			"aks-test-3",
			"aks:test-3",
			false,
		},
		{
			"random name",
			"youthful_turing",
			"youthful_turing",
			false,
		},
		{
			"random with aks prefix",
			"aksyouthful_turing",
			"aksyouthful_turing",
			false,
		},
		{
			"cluster with arn",
			"arnno",
			"arnno",
			false,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			assert.Equal(t, test.PrettyClusterName,
				controllers.PrettifyClusterName(test.ClusterName, test.Expanded))
		})
	}
}
