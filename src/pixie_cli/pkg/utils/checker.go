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

package utils

import (
	"errors"
	"fmt"
	"strings"

	"github.com/blang/semver"
)

// Contains utilities to check the K8s cluster.

// Checker is a named check.
type Checker interface {
	// Returns the name of the Checker.
	Name() string
	// Checker returns nil for pass, or error.
	Check() error
}

type namedCheck struct {
	name  string
	check func() error
}

func (c *namedCheck) Name() string {
	return c.name
}

func (c *namedCheck) Check() error {
	return c.check()
}

// NamedCheck is used to easily create a Checker with a name.
func NamedCheck(name string, check func() error) Checker {
	return &namedCheck{name: name, check: check}
}

// VersionCompatible checks to make sure version >= minVersion as per semver.
func VersionCompatible(version string, minVersion string) (bool, error) {
	// We don't actually care about pre-release tags, so drop them since they sometimes cause parse error.
	sp := strings.Split(version, "-")
	if len(sp) == 0 {
		return false, errors.New("Failed to parse version string")
	}
	version = sp[0]
	version = strings.TrimPrefix(version, "v")
	// Minor version can sometime contain a "+", we remove it so it parses properly with semver.
	version = strings.TrimSuffix(version, "+")
	minVersion = strings.TrimPrefix(minVersion, "v")
	v, err := semver.Make(version)
	if err != nil {
		return false, err
	}
	vMin, err := semver.Make(minVersion)
	if err != nil {
		return false, err
	}

	return v.GE(vMin), nil
}

type jobAdapter struct {
	Checker
}

func checkWrapper(check Checker) jobAdapter {
	return jobAdapter{check}
}

func (j jobAdapter) Run() error {
	return j.Check()
}

// RunClusterChecks will run a list of checks and print out their results.
// The first error is returned, but we continue to run all checks.
func RunClusterChecks(checks []Checker) error {
	jobs := make([]Task, len(checks))
	for i, check := range checks {
		jobs[i] = checkWrapper(check)
	}
	jr := NewSerialTaskRunner(jobs)
	return jr.RunAndMonitor()
}

// RunDefaultClusterChecks runs the default configured checks.
func RunDefaultClusterChecks() error {
	fmt.Printf("\nRunning Cluster Checks:\n")
	return RunClusterChecks(DefaultClusterChecks)
}

// RunExtraClusterChecks runs the extra configured checks.
func RunExtraClusterChecks() error {
	return RunClusterChecks(ExtraClusterChecks)
}
