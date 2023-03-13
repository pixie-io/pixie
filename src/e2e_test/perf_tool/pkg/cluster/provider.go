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

package cluster

import (
	"context"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
)

// Provider is an interface for implementations that can provide a cluster conforming to a given ClusterSpec.
type Provider interface {
	// GetCluster returns the Context for a cluster matching the given spec, a cleanup function for tearing down the cluster, or an error on failure.
	// The caller is responsible for calling Context.Close
	GetCluster(context.Context, *experimentpb.ClusterSpec) (*Context, func(), error)
	// Close cleanups the provider.
	Close() error
}
