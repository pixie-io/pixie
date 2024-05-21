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

package gke

import (
	"context"
	"crypto/rand"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/cenkalti/backoff/v4"
	log "github.com/sirupsen/logrus"
	containerpb "google.golang.org/genproto/googleapis/container/v1"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

const (
	defaultClusterNamePrefix               = `perf-exp-cluster`
	defaultNameRandomSize                  = 4
	defaultGCloudOperationTimeout          = 45 * time.Minute
	defaultGCloudOperationPollPeriod       = 30 * time.Second
	defaultKubeInternalsHealthcheckTimeout = 10 * time.Minute
	clusterLabelKey                        = "px-perf-exp-cluster"
)

func (p *ClusterProvider) clusterParentPath() string {
	return fmt.Sprintf("projects/%s/locations/%s", p.clusterOpts.Project, p.clusterOpts.Zone)
}

func (p *ClusterProvider) fullNameForCluster(c *containerpb.Cluster) string {
	return fmt.Sprintf("%s/clusters/%s", p.clusterParentPath(), c.Name)
}

func (p *ClusterProvider) getClusterContext(ctx context.Context, c *containerpb.Cluster) (*cluster.Context, error) {
	ca := c.MasterAuth.ClusterCaCertificate
	endpoint := fmt.Sprintf("https://%s", c.Endpoint)
	kubeconfig, err := fillKubeconfigTemplate(c.Name, ca, endpoint)
	if err != nil {
		return nil, err
	}
	clusterCtx, err := cluster.NewContextFromConfig(kubeconfig)
	if err != nil {
		return nil, err
	}
	return clusterCtx, nil
}

func (p *ClusterProvider) waitForKubeInternals(ctx context.Context, c *containerpb.Cluster, clusterCtx *cluster.Context) error {
	expBackoff := backoff.NewExponentialBackOff()
	expBackoff.InitialInterval = time.Second
	expBackoff.MaxElapsedTime = defaultKubeInternalsHealthcheckTimeout
	bo := backoff.WithContext(expBackoff, ctx)
	op := func() error {
		namespaces, err := clusterCtx.Clientset().CoreV1().Namespaces().List(ctx, metav1.ListOptions{})
		if err != nil {
			return err
		}
		for _, ns := range namespaces.Items {
			if !isKubeInternalNamespace(ns.Name) {
				continue
			}
			pl, err := clusterCtx.Clientset().CoreV1().Pods(ns.Name).List(ctx, metav1.ListOptions{})
			if err != nil {
				return err
			}
			for _, pod := range pl.Items {
				for _, cs := range pod.Status.InitContainerStatuses {
					if cs.State.Terminated == nil {
						return fmt.Errorf(
							"pod '%s' in namespace '%s' has unfinished init container '%s'",
							pod.Name,
							ns.Name,
							cs.Name,
						)
					}
				}
				for _, cs := range pod.Status.ContainerStatuses {
					if !cs.Ready {
						return fmt.Errorf(
							"pod '%s' in namespace '%s' has not ready container '%s'",
							pod.Name,
							ns.Name,
							cs.Name,
						)
					}
				}
			}
		}
		// Make sure all the kube-system resources are ready. Without this we see errors like:
		// "couldn't get resource list for metrics.k8s.io/v1beta1: the server is currently unable to handle the request"
		_, err = clusterCtx.Clientset().Discovery().ServerPreferredResources()
		if err != nil {
			return err
		}

		return nil
	}
	notify := func(err error, dur time.Duration) {
		log.WithField("cluster", c.Name).WithError(err).Tracef("k8s internals not setup yet, retrying in %v", dur.Round(time.Second))
	}
	return backoff.RetryNotify(op, bo, notify)
}

func isKubeInternalNamespace(ns string) bool {
	return strings.HasPrefix(ns, "kube-")
}

func (p *ClusterProvider) deleteClusterCleanupFunc(c *containerpb.Cluster) func() {
	return func() {
		log.WithField("cluster_name", c.Name).Trace("Deleting cluster")
		err := p.waitForOpWithRetries(context.Background(), func() (*containerpb.Operation, error) {
			return p.client.DeleteCluster(context.Background(), &containerpb.DeleteClusterRequest{
				Name: p.fullNameForCluster(c),
			})
		})
		if err != nil {
			log.WithError(err).Error("failed to delete cluster")
		}
	}
}

func nodePoolNameFromSpec(spec *experimentpb.ClusterSpec) string {
	return spec.Node.MachineType
}

func (p *ClusterProvider) nodePoolForSpec(spec *experimentpb.ClusterSpec) *containerpb.NodePool {
	return &containerpb.NodePool{
		Name: nodePoolNameFromSpec(spec),
		Config: &containerpb.NodeConfig{
			MachineType: spec.Node.MachineType,
			DiskSizeGb:  p.clusterOpts.DiskSizeGB,
			DiskType:    p.clusterOpts.DiskType,
			OauthScopes: []string{
				"https://www.googleapis.com/auth/devstorage.read_only",
				"https://www.googleapis.com/auth/logging.write",
				"https://www.googleapis.com/auth/monitoring",
				"https://www.googleapis.com/auth/service.management.readonly",
				"https://www.googleapis.com/auth/servicecontrol",
				"https://www.googleapis.com/auth/trace.append",
				"https://www.googleapis.com/auth/compute",
			},
			Labels: map[string]string{
				clusterLabelKey: "",
			},
		},
		InitialNodeCount: spec.NumNodes,
		Autoscaling: &containerpb.NodePoolAutoscaling{
			Enabled: false,
		},
		Management: &containerpb.NodeManagement{
			AutoUpgrade: false,
			AutoRepair:  false,
		},
	}
}

func (p *ClusterProvider) createCluster(ctx context.Context, spec *experimentpb.ClusterSpec) (*containerpb.Cluster, error) {
	randBytes := make([]byte, defaultNameRandomSize)
	_, err := rand.Read(randBytes)
	if err != nil {
		return nil, err
	}
	name := fmt.Sprintf("%s-%x", defaultClusterNamePrefix, randBytes)
	nodePools := make([]*containerpb.NodePool, 1)
	nodePools[0] = p.nodePoolForSpec(spec)

	log.WithField("cluster_name", name).Trace("Creating cluster")

	err = p.waitForOpWithRetries(ctx, func() (*containerpb.Operation, error) {
		return p.client.CreateCluster(ctx, &containerpb.CreateClusterRequest{
			Cluster: &containerpb.Cluster{
				Name:            name,
				Description:     "Cluster for running performance experiment",
				Network:         p.clusterOpts.Network,
				Subnetwork:      p.clusterOpts.Subnet,
				ClusterIpv4Cidr: p.clusterOpts.CIDR,
				Autoscaling: &containerpb.ClusterAutoscaling{
					EnableNodeAutoprovisioning: false,
				},
				NodePools: nodePools,
				Location:  p.clusterParentPath(),
				AddonsConfig: &containerpb.AddonsConfig{
					HttpLoadBalancing: &containerpb.HttpLoadBalancing{
						Disabled: false,
					},
					HorizontalPodAutoscaling: &containerpb.HorizontalPodAutoscaling{
						Disabled: false,
					},
				},
				LoggingService:    `none`,
				MonitoringService: `none`,
				IpAllocationPolicy: &containerpb.IPAllocationPolicy{
					UseIpAliases:          true,
					CreateSubnetwork:      p.clusterOpts.DynamicSubnet,
					ClusterIpv4CidrBlock:  p.clusterOpts.CIDR,
					ServicesIpv4CidrBlock: p.clusterOpts.ServicesCIDR,
				},
				MasterAuth: &containerpb.MasterAuth{},
				AuthenticatorGroupsConfig: &containerpb.AuthenticatorGroupsConfig{
					Enabled:       true,
					SecurityGroup: p.clusterOpts.SecurityGroup,
				},
				ResourceLabels: map[string]string{
					clusterLabelKey: "",
				},
			},
			Parent: p.clusterParentPath(),
		})
	})
	if err != nil {
		return nil, err
	}

	c, err := p.client.GetCluster(ctx, &containerpb.GetClusterRequest{
		Name: fmt.Sprintf("%s/clusters/%s", p.clusterParentPath(), name),
	})
	if err != nil {
		return nil, err
	}

	return c, nil
}

func (p *ClusterProvider) waitForOp(ctx context.Context, op *containerpb.Operation) error {
	t := time.NewTimer(defaultGCloudOperationTimeout)
	for op.Status != containerpb.Operation_DONE {
		// Poll only every poll period.
		time.Sleep(defaultGCloudOperationPollPeriod)
		// Check that we haven't reached the deadline.
		select {
		case <-t.C:
			err := p.cancelOp(ctx, op)
			if err != nil {
				log.WithError(err).WithField("op", op).Error("failed to cancel operation")
			}
			return errors.New("timed out waiting for operation")
		default:
			break
		}
		// Call out to gcloud to update the operation status.
		var err error
		op, err = p.client.GetOperation(ctx, &containerpb.GetOperationRequest{
			Name: fmt.Sprintf("%s/operations/%s", p.clusterParentPath(), op.Name),
		})
		if err != nil {
			return err
		}
	}
	return nil
}

func (p *ClusterProvider) cancelOp(ctx context.Context, op *containerpb.Operation) error {
	return p.client.CancelOperation(ctx, &containerpb.CancelOperationRequest{
		Name: fmt.Sprintf("%s/operations/%s", p.clusterParentPath(), op.Name),
	})
}

func (p *ClusterProvider) waitForOpWithRetries(ctx context.Context, opFunc func() (*containerpb.Operation, error)) error {
	var op *containerpb.Operation
	funcToRetry := func() error {
		var err error
		op, err = opFunc()
		if err != nil {
			s, ok := status.FromError(err)
			if ok && (s.Code() == codes.Unavailable || s.Code() == codes.FailedPrecondition) {
				// Gcloud uses FailedPrecondition when a cluster is being operated on by another operation, so we retry for that code as well as unavailable.
				return err
			}
			// Any other code, and we don't bother retrying.
			return backoff.Permanent(err)
		}
		return nil
	}

	expBackoff := backoff.NewExponentialBackOff()
	expBackoff.InitialInterval = 1 * time.Minute
	expBackoff.Multiplier = 2
	expBackoff.MaxElapsedTime = defaultGCloudOperationTimeout
	bo := backoff.WithContext(expBackoff, ctx)

	err := backoff.Retry(funcToRetry, bo)
	if err != nil {
		return err
	}

	err = p.waitForOp(ctx, op)
	if err != nil {
		return err
	}
	return nil
}
