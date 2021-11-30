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

	"github.com/gogo/protobuf/types"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/vzmgr/controllers"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/k8s/metadatapb"
)

func TestPodStatusScan(t *testing.T) {
	var inputPodStatuses controllers.PodStatuses = map[string]*cvmsgspb.PodStatus{
		"vizier-proxy": {
			Name:          "vizier-proxy",
			Status:        metadatapb.RUNNING,
			StatusMessage: "a message",
			Reason:        "a reason",
			Containers: []*cvmsgspb.ContainerStatus{
				{
					Name:    "a-container",
					State:   metadatapb.CONTAINER_STATE_RUNNING,
					Message: "a container message",
					Reason:  "a container reason",
					CreatedAt: &types.Timestamp{
						Seconds: 1599694225000,
						Nanos:   124,
					},
				},
			},
			CreatedAt: &types.Timestamp{
				Seconds: 1599694225000,
				Nanos:   0,
			},
		},
		"vizier-query-broker": {
			Name:   "vizier-query-broker",
			Status: metadatapb.RUNNING,
			CreatedAt: &types.Timestamp{
				Seconds: 1599684335224,
				Nanos:   42342,
			},
		},
	}

	var outputPodStatuses controllers.PodStatuses

	serialized, err := inputPodStatuses.Value()
	require.NoError(t, err)

	err = outputPodStatuses.Scan(serialized)
	require.NoError(t, err)

	assert.Equal(t, inputPodStatuses, outputPodStatuses)
}
