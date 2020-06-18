package utils

import (
	"context"
	"errors"
	"fmt"
	"os/exec"
	"strings"

	"gopkg.in/yaml.v2"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

const (
	k8sMinVersion     = "1.12.0"
	kubectlMinVersion = "1.10.0"
	kernelMinVersion  = "4.14.0"
)

var (
	kernelVersionCheck = NamedCheck(fmt.Sprintf("Kernel version > %s", kernelMinVersion), func() error {
		kubeConfig := k8s.GetConfig()
		clientset := k8s.GetClientset(kubeConfig)
		nodes, err := clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
		if err != nil {
			return err
		}

		for _, node := range nodes.Items {
			compatible, err := VersionCompatible(node.Status.NodeInfo.KernelVersion, kernelMinVersion)
			if err != nil {
				return err
			}
			if !compatible {
				return fmt.Errorf("kernel version for node (%s) not supported. Must have minimum kernel version of (%s)", node.Name, kernelMinVersion)
			}
		}
		return nil
	})
	k8sVersionCheck = NamedCheck(fmt.Sprintf("K8s version > %s", k8sMinVersion), func() error {
		kubeConfig := k8s.GetConfig()

		discoveryClient := k8s.GetDiscoveryClient(kubeConfig)
		version, err := discoveryClient.ServerVersion()
		if err != nil {
			return err
		}
		compatible, err := VersionCompatible(version.GitVersion, k8sMinVersion)
		if err != nil {
			return err
		}
		if !compatible {
			return fmt.Errorf("k8s version (%s) not supported. Must have minimum k8s version of (%s)", version.GitVersion, k8sMinVersion)
		}
		return nil
	})
	hasKubectlCheck = NamedCheck(fmt.Sprintf("Kubectl > %s is present", kubectlMinVersion), func() error {
		result, err := exec.Command("kubectl", "version", "-o", "yaml").Output()
		if err != nil {
			return err
		}

		var version struct {
			ClientVersion struct {
				Major string
				Minor string
			} `yaml:"clientVersion"`
		}
		err = yaml.Unmarshal(result, &version)
		if err != nil {
			return err
		}

		minorVersion := strings.TrimSuffix(version.ClientVersion.Minor, "+")
		kubectlVersion := fmt.Sprintf("%s.%s.0", version.ClientVersion.Major, minorVersion)
		compatible, err := VersionCompatible(kubectlVersion, kubectlMinVersion)
		if err != nil {
			return err
		}
		if !compatible {
			return fmt.Errorf("kubectl version (%s) not supported. Must have minimum kubectl version of (%s)", kubectlVersion, kubectlMinVersion)
		}
		return nil
	})
	userCanCreateNamespace = NamedCheck("User can create namespace", func() error {
		result, err := exec.Command("kubectl", "auth", "can-i", "create", "namespace").Output()
		if err != nil {
			return err
		}
		s := string(result)
		if strings.TrimSpace(s) != "yes" {
			return errors.New("user does not have permission to create namespace")
		}
		return nil
	})
)

// DefaultClusterChecks is a list of cluster that are performed by default.
var DefaultClusterChecks = []Checker{
	kernelVersionCheck,
	k8sVersionCheck,
	hasKubectlCheck,
	userCanCreateNamespace,
}
