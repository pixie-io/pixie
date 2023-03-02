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

package steps

import (
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

// DeployStep is the interface for running a stage in a given workloads deploy process.
type DeployStep interface {
	// Prepare runs any preparation steps that don't require an assigned cluster.
	Prepare() error
	// Deploy runs the deploy step, and returns any namespaces that should be deleted on Close.
	Deploy(*cluster.Context) ([]string, error)
	// Name returns a printable name for the deploy step.
	Name() string
}
