package controller_test

import (
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/vzmgr/controller"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/k8s/metadatapb"
)

func TestPodStatusScan(t *testing.T) {
	var inputPodStatuses controller.PodStatuses = map[string]*cvmsgspb.PodStatus{
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

	var outputPodStatuses controller.PodStatuses

	serialized, err := inputPodStatuses.Value()
	require.NoError(t, err)

	err = outputPodStatuses.Scan(serialized)
	require.NoError(t, err)

	assert.Equal(t, inputPodStatuses, outputPodStatuses)
}
