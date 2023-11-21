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

package suites

import (
	// Embed import is required to use go:embed directive.
	_ "embed"
	"fmt"
	"strings"
	"text/template"

	log "github.com/sirupsen/logrus"

	pb "px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
)

// VizierWorkload returns the workload spec to deploy Vizier.
func VizierWorkload() *pb.WorkloadSpec {
	return &pb.WorkloadSpec{
		Name: "vizier",
		DeploySteps: []*pb.DeployStep{
			{
				DeployType: &pb.DeployStep_Px{
					Px: &pb.PxCLIDeploy{
						Args: []string{
							"deploy",
						},
						SetClusterID: true,
						Namespaces: []string{
							"pl",
							"px-operator",
							"olm",
						},
					},
				},
			},
			{
				DeployType: &pb.DeployStep_Px{
					Px: &pb.PxCLIDeploy{
						Args: []string{
							"delete",
							"--clobber=false",
						},
					},
				},
			},
			{
				DeployType: &pb.DeployStep_Skaffold{
					Skaffold: &pb.SkaffoldDeploy{
						SkaffoldPath: "skaffold/skaffold_vizier.yaml",
						SkaffoldArgs: []string{
							"-p", "opt",
						},
					},
				},
			},
		},
		Healthchecks: VizierHealthChecks(),
	}
}

const protocolLoadtestConfigMapPatch = `
apiVersion: v1
kind: ConfigMap
metadata:
  name: px-protocol-loadtest-config
data:
  NUM_CONNECTIONS: "%d"
  TARGET_RPS: "%d"
`

// HTTPLoadTestWorkload returns the WorkloadSpec to deploy a simple client/server http loadtest (see src/e2e_test/protocol_loadtest)
func HTTPLoadTestWorkload(numConns int, targetRPS int, doPxLHealthCheck bool) *pb.WorkloadSpec {
	return &pb.WorkloadSpec{
		Name: "http_loadtest",
		DeploySteps: []*pb.DeployStep{
			{
				DeployType: &pb.DeployStep_Skaffold{
					Skaffold: &pb.SkaffoldDeploy{
						SkaffoldPath: "src/e2e_test/protocol_loadtest/skaffold_loadtest.yaml",
					},
				},
			},
			{
				DeployType: &pb.DeployStep_Skaffold{
					Skaffold: &pb.SkaffoldDeploy{
						SkaffoldPath: "src/e2e_test/protocol_loadtest/skaffold_client.yaml",
						Patches: []*pb.PatchSpec{
							{
								YAML: fmt.Sprintf(protocolLoadtestConfigMapPatch, numConns, targetRPS),
								Target: &pb.PatchTarget{
									Kind: "ConfigMap",
									Name: "px-protocol-loadtest-config",
								},
							},
						},
					},
				},
			},
		},
		Healthchecks: HTTPHealthChecks("px-protocol-loadtest", doPxLHealthCheck),
	}
}

// SockShopWorkload returns the WorkloadSpec to deploy sock shop.
func SockShopWorkload() *pb.WorkloadSpec {
	return &pb.WorkloadSpec{
		Name: "px-sock-shop",
		DeploySteps: []*pb.DeployStep{
			{
				DeployType: &pb.DeployStep_Px{
					Px: &pb.PxCLIDeploy{
						Args: []string{
							"demo",
							"deploy",
							"px-sock-shop",
						},
						Namespaces: []string{
							"px-sock-shop",
						},
					},
				},
			},
		},
		Healthchecks: HTTPHealthChecks("px-sock-shop", true),
	}
}
func K8ssandraWorkload() *pb.WorkloadSpec {
	return &pb.WorkloadSpec{
		Name: "px-python-demo",
		DeploySteps: []*pb.DeployStep{
			{
				DeployType: &pb.DeployStep_Px{
					Px: &pb.PxCLIDeploy{
						Args: []string{
							"demo",
							"deploy",
							"px-k8ssandra",
						},
						Namespaces: []string{
							"px-k8ssandra",
						},
					},
				},
			},
		},
		Healthchecks: HTTPHealthChecks("px-k8ssandra", true),
	}
}

// OnlineBoutiqueWorkload returns the WorkloadSpec to deploy online boutique.
func OnlineBoutiqueWorkload() *pb.WorkloadSpec {
	return &pb.WorkloadSpec{
		Name: "px-online-boutique",
		DeploySteps: []*pb.DeployStep{
			{
				DeployType: &pb.DeployStep_Px{
					Px: &pb.PxCLIDeploy{
						Args: []string{
							"demo",
							"deploy",
							"px-online-boutique",
						},
						Namespaces: []string{
							"px-online-boutique",
						},
					},
				},
			},
		},
		Healthchecks: HTTPHealthChecks("px-online-boutique", true),
	}
}

// KafkaWorkload returns the WorkloadSpec to deploy the kafka demo.
func KafkaWorkload() *pb.WorkloadSpec {
	return &pb.WorkloadSpec{
		Name: "px-kafka",
		DeploySteps: []*pb.DeployStep{
			{
				DeployType: &pb.DeployStep_Px{
					Px: &pb.PxCLIDeploy{
						Args: []string{
							"demo",
							"deploy",
							"px-kafka",
						},
						Namespaces: []string{
							"px-kafka",
						},
					},
				},
			},
		},
		Healthchecks: HTTPHealthChecks("px-kafka", true),
	}
}

//go:embed scripts/healthcheck/vizier.pxl
var vizierHealthCheckScript string

//go:embed scripts/healthcheck/http_data_in_namespace.pxl
var httpHealthCheckScript string

// VizierHealthChecks returns the healthchecks for a vizier workload.
func VizierHealthChecks() []*pb.HealthCheck {
	return []*pb.HealthCheck{
		{
			CheckType: &pb.HealthCheck_K8S{
				K8S: &pb.K8SPodsReadyCheck{
					Namespace: "pl",
				},
			},
		},
		{
			CheckType: &pb.HealthCheck_PxL{
				PxL: &pb.PxLHealthCheck{
					Script:        vizierHealthCheckScript,
					SuccessColumn: "success",
				},
			},
		},
	}
}

// HTTPHealthChecks returns a healthcheck based on the existence of http_events in Pixie for a given namespace.
func HTTPHealthChecks(namespace string, includePxL bool) []*pb.HealthCheck {
	checks := []*pb.HealthCheck{
		{
			CheckType: &pb.HealthCheck_K8S{
				K8S: &pb.K8SPodsReadyCheck{
					Namespace: namespace,
				},
			},
		},
	}
	if includePxL {
		t, err := template.New("").Parse(httpHealthCheckScript)
		if err != nil {
			log.WithError(err).Fatal("failed to parse HTTP healthcheck script")
		}
		buf := &strings.Builder{}
		err = t.Execute(buf, &struct {
			Namespace string
		}{
			Namespace: namespace,
		})
		if err != nil {
			log.WithError(err).Fatal("failed to execute HTTP healthcheck template")
		}
		checks = append(checks, &pb.HealthCheck{
			CheckType: &pb.HealthCheck_PxL{
				PxL: &pb.PxLHealthCheck{
					Script:        buf.String(),
					SuccessColumn: "success",
				},
			},
		})
	}

	return checks
}
