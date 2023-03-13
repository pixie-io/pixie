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
	"bytes"
	"fmt"
	"os"
	"strings"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

type prerenderedDeployImpl struct {
	spec *experimentpb.PrerenderedDeploy

	r *renderedYAML
}

var _ DeployStep = &prerenderedDeployImpl{}

// NewPrerenderedDeploy creates a new DeployStep which deploys a prerendered set of yamls.
func NewPrerenderedDeploy(spec *experimentpb.PrerenderedDeploy) DeployStep {
	return &prerenderedDeployImpl{
		spec: spec,
	}
}

// Name returns a name for this deploy step.
func (p *prerenderedDeployImpl) Name() string {
	return fmt.Sprintf("prerendered deploy %s", strings.Join(p.spec.YAMLPaths, ", "))
}

// Prepare concatenates all the YAMLs and then applies any patches from the spec.
func (p *prerenderedDeployImpl) Prepare() error {
	buf := bytes.Buffer{}
	for _, path := range p.spec.YAMLPaths {
		b, err := os.ReadFile(path)
		if err != nil {
			return err
		}
		if _, err := buf.Write(b); err != nil {
			return err
		}
	}

	p.r = &renderedYAML{
		yaml: buf.Bytes(),
	}
	if err := p.r.patch(p.spec.Patches); err != nil {
		return err
	}
	return nil
}

// Deploy deploys the rendered and patched yaml.
func (p *prerenderedDeployImpl) Deploy(clusterCtx *cluster.Context) ([]string, error) {
	if err := p.r.deploy(clusterCtx); err != nil {
		return nil, err
	}
	ns, err := p.r.getNamespace()
	if err != nil {
		return nil, err
	}
	return []string{ns}, nil
}
