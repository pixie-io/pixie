package controller_test

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/api/controller"
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
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			assert.Equal(t, test.PrettyClusterName,
				controller.PrettifyClusterName(test.ClusterName, test.Expanded))
		})
	}
}
