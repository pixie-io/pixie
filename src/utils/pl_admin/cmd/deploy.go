package cmd

import (
	"errors"
	"fmt"
	"strconv"
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"pixielabs.ai/pixielabs/src/utils/pl_admin/cmd/k8s"
)

const (
	k8sMinVersion    = "1.8"
	kernelMinVersion = "4.14"
)

// NewCmdDeploy creates a new "deploy" command.
func NewCmdDeploy() *cobra.Command {
	return &cobra.Command{
		Use:   "deploy",
		Short: "Deploys Pixie on the current K8s cluster",
		Run: func(cmd *cobra.Command, args []string) {
			check, _ := cmd.Flags().GetBool("check")
			if check {
				checkCluster()
				return
			}

			// TODO(michelle): Use extract_yaml and use_version to deploy Pixie.
			_, _ = cmd.Flags().GetString("extract_yaml")
			_, _ = cmd.Flags().GetString("use_version")

			// TODO(michelle): Handle registration key.
			_, _ = cmd.Flags().GetString("registration_key")
		},
	}
}

// VersionCompatible checks whether a version is compatible, given a minimum version.
func VersionCompatible(version string, minVersion string) (bool, error) {
	versionParts := strings.Split(version, ".")
	if len(versionParts) != 2 {
		return false, errors.New("Version string incorrectly formatted")
	}
	major, err := strconv.Atoi(versionParts[0])
	if err != nil {
		return false, errors.New("Could not convert major version from string into int")
	}
	minor, err := strconv.Atoi(versionParts[1])
	if err != nil {
		return false, errors.New("Could not convert minor version from string into int")
	}

	minVersionParts := strings.Split(minVersion, ".")
	if len(minVersionParts) != 2 {
		return false, errors.New("Min version string incorrectly formatted")
	}
	minMajor, err := strconv.Atoi(minVersionParts[0])
	if err != nil {
		return false, errors.New("Could not convert min major version from string into int")
	}
	minMinor, err := strconv.Atoi(minVersionParts[1])
	if err != nil {
		return false, errors.New("Could not convert min minor version from string into int")
	}

	if (major < minMajor) || (major == minMajor && minor < minMinor) {
		return false, nil
	}
	return true, nil
}

// checkCluster checks whether Pixie can properly run on the user's cluster.
func checkCluster() {
	kubeConfig := k8s.GetConfig()

	// Check K8s server version.
	discoveryClient := k8s.GetDiscoveryClient(kubeConfig)
	version, err := discoveryClient.ServerVersion()
	if err != nil {
		log.WithError(err).Fatal("Could not get k8s server version")
	}
	log.Info(fmt.Sprintf("Kubernetes server version: %s.%s", version.Major, version.Minor))
	compatible, err := VersionCompatible(fmt.Sprintf("%s.%s", version.Major, version.Minor), k8sMinVersion)
	if err != nil {
		log.WithError(err).Fatal("Could not check k8s server version")
	}
	if !compatible {
		log.Info(fmt.Sprintf("Kubernetes server version not supported. Must have minimum version of %s", k8sMinVersion))
	}

	// Check version of each node.
	clientset := k8s.GetClientset(kubeConfig)
	nodes, err := clientset.CoreV1().Nodes().List(metav1.ListOptions{})
	log.Info(fmt.Sprintf("Checking kernel versions of nodes. Found %d node(s)", len(nodes.Items)))
	for _, node := range nodes.Items {
		compatible, err := VersionCompatible(node.Status.NodeInfo.KernelVersion, kernelMinVersion)
		if err != nil {
			log.WithError(err).Fatal("Could not check node kernel version")
		}
		if !compatible {
			log.Info(fmt.Sprintf("Kernel version for node %s not supported. Must have minimum kernel version of %s", node.Name, kernelMinVersion))
		}
	}
}
