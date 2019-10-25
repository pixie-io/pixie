package k8s

import (
	"fmt"
	"strings"
	"time"

	"github.com/blang/semver"
	"github.com/briandowns/spinner"
	"github.com/fatih/color"
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

// RunClusterChecks will run a list of checks and print out their results.
// The first error is returned, but we continue to run all checks.
func RunClusterChecks(checks []Checker) error {
	fmt.Printf("\nRunning Cluster Checks:\n")
	var finalErr error
	for _, check := range checks {
		err := func() error {
			s := spinner.New(spinner.CharSets[14], 100*time.Millisecond)
			defer s.Stop()
			s.Prefix = "\t"
			s.Suffix = fmt.Sprintf(" : %s", check.Name())
			s.FinalMSG = fmt.Sprintf("\t%s : %s\n", color.GreenString("\u2713"), check.Name())
			s.Start()

			err := check.Check()
			if err != nil {
				s.FinalMSG = fmt.Sprintf("\t%s : %s, error=%s\n", color.RedString("\u2715"), check.Name(), err.Error())
			}

			return nil
		}()
		if err != nil && finalErr == nil {
			// Return the first error, but keep running the checks.
			finalErr = err
		}
	}
	return finalErr
}

// RunDefaultClusterChecks runs the default configured checks.
func RunDefaultClusterChecks() error {
	return RunClusterChecks(DefaultClusterChecks)
}
