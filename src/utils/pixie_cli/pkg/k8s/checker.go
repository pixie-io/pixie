package k8s

import (
	"fmt"
	"strings"
	"time"

	"github.com/blang/semver"
	"github.com/briandowns/spinner"
	"github.com/fatih/color"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// Contains utilities to check the K8s cluster.

const (
	k8sMinVersion    = "1.8.0"
	kernelMinVersion = "4.14.0"
)

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

var defaultChecks = []Checker{
	NamedCheck(fmt.Sprintf("Kernel version > %s", kernelMinVersion), func() error {
		kubeConfig := GetConfig()
		clientset := GetClientset(kubeConfig)
		nodes, err := clientset.CoreV1().Nodes().List(metav1.ListOptions{})
		if err != nil {
			return err
		}

		for _, node := range nodes.Items {
			compatible, err := versionCompatible(node.Status.NodeInfo.KernelVersion, kernelMinVersion)
			if err != nil {
				return err
			}
			if !compatible {
				return fmt.Errorf("kernel version for node %s not supported. Must have minimum kernel version of %s", node.Name, kernelMinVersion)
			}
		}
		return nil
	}),
	NamedCheck(fmt.Sprintf("K8s version > %s", k8sMinVersion), func() error {
		kubeConfig := GetConfig()

		discoveryClient := GetDiscoveryClient(kubeConfig)
		version, err := discoveryClient.ServerVersion()
		if err != nil {
			return err
		}

		compatible, err := versionCompatible(version.GitVersion, k8sMinVersion)
		if err != nil {
			return err
		}
		if !compatible {
			return fmt.Errorf("k8s version not supported. Must have minimum k8s version of %s", version.GitVersion, k8sMinVersion)
		}

		return nil
	}),
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
	return RunClusterChecks(defaultChecks)
}
