package k8s

import (
	"fmt"
	"strings"

	"github.com/blang/semver"
	cliutils "pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
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

func versionCompatible(version string, minVersion string) (bool, error) {
	version = strings.TrimPrefix(version, "v")
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
	fmt.Printf("\nRunning Cluster Checks:\n")
	jobs := make([]cliutils.Task, len(checks))
	for i, check := range checks {
		jobs[i] = checkWrapper(check)
	}
	jr := cliutils.NewSerialTaskRunner(jobs)
	return jr.RunAndMonitor()
}

// RunDefaultClusterChecks runs the default configured checks.
func RunDefaultClusterChecks() error {
	return RunClusterChecks(DefaultClusterChecks)
}
