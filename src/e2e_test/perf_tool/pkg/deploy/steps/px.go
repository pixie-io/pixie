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
	"fmt"
	"strings"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
)

type pxDeployImpl struct {
	pxCtx *pixie.Context
	spec  *experimentpb.PxCLIDeploy
}

var _ DeployStep = &pxDeployImpl{}

// NewPxDeploy creates a new DeployStep that deploys some part of a workload to the cluster using the PX CLI.
func NewPxDeploy(pxCtx *pixie.Context, spec *experimentpb.PxCLIDeploy) DeployStep {
	return &pxDeployImpl{
		pxCtx: pxCtx,
		spec:  spec,
	}
}

// Name returns a printable name for this deploy step.
func (px *pxDeployImpl) Name() string {
	return fmt.Sprintf("px %s", strings.Join(px.spec.Args, " "))
}

// Prepare doesn't do anything for the px deploy step.
func (px *pxDeployImpl) Prepare() error {
	// px deploy / px demo deploy can't do anything without the clusterCtx.
	return nil
}

func hasElem(args []string, arg string) bool {
	for _, a := range args {
		if a == arg {
			return true
		}
	}
	return false
}

// Deploy runs makes sure `px` is auth'd, then runs the `px` command specified in the spec.
func (px *pxDeployImpl) Deploy(clusterCtx *cluster.Context) ([]string, error) {
	if _, err := px.pxCtx.RunPXCmd(clusterCtx, "auth", "login", "--use_api_key=true"); err != nil {
		return nil, err
	}
	args := px.spec.Args
	// Make sure that the px deploy command doesn't prompt for user input.
	if hasElem(args, "deploy") && !hasElem(args, "-y") {
		args = append(args, "-y")
	}
	if _, err := px.pxCtx.RunPXCmd(clusterCtx, args...); err != nil {
		return nil, err
	}
	if px.spec.SetClusterID {
		clusterIDBytes, err := px.pxCtx.RunPXCmd(clusterCtx, "get", "cluster", "--id")
		if err != nil {
			return nil, err
		}
		clusterIDStr := strings.Trim(string(clusterIDBytes), " \n")
		id, err := uuid.FromString(clusterIDStr)
		if err != nil {
			return nil, err
		}
		px.pxCtx.SetClusterID(id)
	}
	// We don't know what namespaces a given `px` command will create, so we rely on the user to set them in the spec.
	return px.spec.Namespaces, nil
}
