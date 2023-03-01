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

package local

import (
	"context"
	"errors"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/utils/shared/k8s"
)

// ClusterProvider uses whatever cluster your local kubeconfig points to.
type ClusterProvider struct {
}

var _ cluster.Provider = &ClusterProvider{}

// GetCluster ignores the passed in `spec` and returns a `cluster.Context` pointing to your local kubeconfig.
// The kubeconfig defaults to `~/.kube/config` and can be changed with the KUBECONFIG environment variable, or `--kubeconfig` on the command line.
func (p *ClusterProvider) GetCluster(ctx context.Context, spec *experimentpb.ClusterSpec) (*cluster.Context, func(), error) {
	kubeconfigPath := k8s.GetKubeconfigPath()
	if kubeconfigPath == "" {
		return nil, nil, errors.New("No kubeconfig path set, can't use local cluster config. Set KUBECONFIG or --kubeconfig")
	}
	clusterCtx, err := cluster.NewContextFromPath(kubeconfigPath)
	if err != nil {
		return nil, nil, err
	}
	cleanup := func() {}
	return clusterCtx, cleanup, nil
}

// Close does nothing for this cluster provider.
func (p *ClusterProvider) Close() error {
	return nil
}
