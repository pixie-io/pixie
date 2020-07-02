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
	}{
		{
			"basic GKE",
			"gke_pl-dev-infra_us-west1-a_jenkins-test-cluster_1",
			"gke:jenkins-test-cluster_1",
		},
		{
			"basic eks",
			"arn:aws:eks:us-east-2:016013129672:cluster/skylab4-hulu",
			"eks:skylab4-hulu",
		},
		{
			"basic aks",
			"aks-test-3",
			"aks:test-3",
		},
		{
			"random name",
			"youthful_turing",
			"youthful_turing",
		},
		{
			"random with aks prefix",
			"aksyouthful_turing",
			"aksyouthful_turing",
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			assert.Equal(t, test.PrettyClusterName,
				controller.PrettifyClusterName(test.ClusterName))
		})
	}
}
