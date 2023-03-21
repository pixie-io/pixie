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
	"os/exec"
	"strings"

	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

type skaffoldDeployImpl struct {
	spec                  *experimentpb.SkaffoldDeploy
	containerRegistryRepo string

	r *renderedYAML
}

var _ DeployStep = &skaffoldDeployImpl{}

// NewSkaffoldDeploy returns a new DeployStep which deploys a stage of a workload using skaffold.
func NewSkaffoldDeploy(spec *experimentpb.SkaffoldDeploy, containerRegistryRepo string) DeployStep {
	return &skaffoldDeployImpl{
		spec:                  spec,
		containerRegistryRepo: containerRegistryRepo,
	}
}

// Name returns a printable name for this deploy step.
func (s *skaffoldDeployImpl) Name() string {
	return fmt.Sprintf("skaffold deploy %s", s.spec.SkaffoldPath)
}

// Prepare runs skaffold to build images and render yamls and then applies any patches from the spec to the rendered yaml.
func (s *skaffoldDeployImpl) Prepare() error {
	buildArtifacts, err := s.runSkaffoldBuild()
	if err != nil {
		return err
	}
	rendered, err := s.runSkaffoldRender(buildArtifacts)
	if err != nil {
		return err
	}
	s.r = &renderedYAML{
		yaml: rendered,
	}
	if err := s.r.patch(s.spec.Patches); err != nil {
		return err
	}
	return nil
}

// Deploy deploys the rendered and patched yaml.
func (s *skaffoldDeployImpl) Deploy(clusterCtx *cluster.Context) ([]string, error) {
	if err := s.r.deploy(clusterCtx); err != nil {
		return nil, err
	}

	ns, err := s.r.getNamespace()
	if err != nil {
		return nil, err
	}
	return []string{ns}, nil
}

func (s *skaffoldDeployImpl) runSkaffoldBuild() ([]byte, error) {
	var buildArtifacts bytes.Buffer
	buildArgs := []string{
		"build",
		"-q",
		"-f", s.spec.SkaffoldPath,
		"-d", s.containerRegistryRepo,
	}
	buildArgs = append(buildArgs, s.spec.SkaffoldArgs...)
	log.Tracef("Running `skaffold %s` ...", strings.Join(buildArgs, " "))
	cmd := exec.Command("skaffold", buildArgs...)
	cmd.Stderr = os.Stderr
	cmd.Stdout = &buildArtifacts
	if err := cmd.Run(); err != nil {
		return nil, fmt.Errorf("failed to run `skaffold %s`: %w", strings.Join(buildArgs, " "), err)
	}
	return buildArtifacts.Bytes(), nil
}

func (s *skaffoldDeployImpl) runSkaffoldRender(buildArtifacts []byte) ([]byte, error) {
	var renderedYAMLs bytes.Buffer
	renderArgs := []string{
		"render",
		"-f", s.spec.SkaffoldPath,
		"--build-artifacts=-",
		"-d", s.containerRegistryRepo,
	}
	renderArgs = append(renderArgs, s.spec.SkaffoldArgs...)
	log.Tracef("Running `skaffold %s` ...", strings.Join(renderArgs, " "))
	cmd := exec.Command("skaffold", renderArgs...)
	cmd.Stdin = bytes.NewReader(buildArtifacts)
	cmd.Stderr = os.Stderr
	cmd.Stdout = &renderedYAMLs
	if err := cmd.Run(); err != nil {
		return nil, fmt.Errorf("failed to run `skaffold %s`: %w", strings.Join(renderArgs, " "), err)
	}

	return renderedYAMLs.Bytes(), nil
}
