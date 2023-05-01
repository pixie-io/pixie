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
	"context"
	"errors"
	"fmt"
	"os/exec"
	"regexp"
	"strings"

	"gopkg.in/yaml.v2"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/utils/shared/k8s"
)

const (
	k8sMinVersion     = "1.16.0"
	kubectlMinVersion = "1.10.0"
	kernelMinVersion  = "4.14.0"
)

// ClusterType represents all possible types of a K8s cluster.
type ClusterType int

const (
	// ClusterTypeUnknown is an unknown cluster type.
	ClusterTypeUnknown ClusterType = iota
	// ClusterTypeGKE is a GKE cluster.
	ClusterTypeGKE
	// ClusterTypeEKS is an EKS cluster.
	ClusterTypeEKS
	// ClusterTypeAKS is an AKS cluster.
	ClusterTypeAKS
	// ClusterTypeKind is a kind cluster.
	ClusterTypeKind
	// ClusterTypeDockerDesktop is a Docker for Desktop cluster.
	ClusterTypeDockerDesktop
	// ClusterTypeMinikubeKVM2 is a minikube cluster running with the kvm2 driver.
	ClusterTypeMinikubeKVM2
	// ClusterTypeMinikubeHyperkit is a minikube cluster running with the hyperkit driver.
	ClusterTypeMinikubeHyperkit
	// ClusterTypeMinikubeOther is a minikube cluster running with a driver not specified in
	// the other enums.
	ClusterTypeMinikubeOther
	// ClusterTypeK0s is a k0s cluster.
	ClusterTypeK0s
	// ClusterTypeK3s is a k3s cluster.
	ClusterTypeK3s
)

var allowedClusterTypes = []ClusterType{
	ClusterTypeGKE,
	ClusterTypeEKS,
	ClusterTypeAKS,
	ClusterTypeMinikubeKVM2,
	ClusterTypeMinikubeHyperkit,
	ClusterTypeK0s,
	ClusterTypeK3s,
}

// detectClusterType gets the cluster type of the cluster for the current kube config context.
func detectClusterType() ClusterType {
	kubeConfig := k8s.GetConfig()
	kubeAPIConfig := k8s.GetClientAPIConfig()
	currentContext := kubeAPIConfig.CurrentContext
	// Get the actual cluster name. The currentContext is currently the context namespace, which usually
	// matches the cluster name, but does not for AKS.
	if n, ok := kubeAPIConfig.Contexts[currentContext]; ok {
		currentContext = n.Cluster
	}

	// Check if it is a kind cluster.
	_, err := exec.Command("/bin/sh", "-c", fmt.Sprintf(`kind get clusters | grep "^$(echo "%s" | sed -e "s/^kind-//g")$"`, currentContext)).Output()
	if err == nil {
		return ClusterTypeKind
	}

	// Check if it a docker-for-desktop cluster.
	s := strings.Trim(currentContext, " \n")
	if s == "docker-desktop" {
		return ClusterTypeDockerDesktop
	}

	// Check if it a minikube cluster.
	result, err := exec.Command("/bin/sh", "-c", fmt.Sprintf(`minikube profile list | grep " %s "| cut -f3 -d'|'`, currentContext)).Output()
	if err == nil {
		s := strings.Trim(string(result), " \n")
		// err not nil, means command failed and either minikube is not installed or not used.
		switch s {
		case "hyperkit":
			return ClusterTypeMinikubeHyperkit
		case "kvm2":
			return ClusterTypeMinikubeKVM2
		case "":
			break
		default:
			return ClusterTypeMinikubeOther
		}
	}

	// Check if it is an aks cluster.
	result, err = exec.Command("/bin/sh", "-c", fmt.Sprintf(`az aks list | grep '"name": "%s"'`, currentContext)).Output()
	if err == nil {
		if len(string(result)) > 0 {
			return ClusterTypeAKS
		}
	}

	// Check if it is an eks/gke cluster. This is done by checking if the version string is in the format of "v1.15.12-gke.2" or "v1.15.11-eks-af3caf".
	discoveryClient := k8s.GetDiscoveryClient(kubeConfig)
	version, err := discoveryClient.ServerVersion()
	if err != nil {
		return ClusterTypeUnknown
	}
	sp := strings.SplitN(version.GitVersion, "-", 2)
	if len(sp) > 1 {
		matchGke, _ := regexp.Match(`gke\..*`, []byte(sp[1]))
		if matchGke {
			return ClusterTypeGKE
		}
		matchEks, _ := regexp.Match(`eks-.*`, []byte(sp[1]))
		if matchEks {
			return ClusterTypeEKS
		}
	}

	sp = strings.SplitN(version.GitVersion, "+", 2)
	if len(sp) > 1 {
		if sp[1] == "k0s" {
			return ClusterTypeK0s
		}
		if strings.HasPrefix(sp[1], "k3s") {
			return ClusterTypeK3s
		}
	}

	return ClusterTypeUnknown
}

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
	clusterTypeIsSupported = NamedCheck("Cluster type is supported", func() error {
		clusterType := detectClusterType()

		switch clusterType {
		case ClusterTypeKind:
			return fmt.Errorf("We don't currently support Kind clusters. To create a test cluster to try out Pixie, use minikube instead.  ")
		case ClusterTypeDockerDesktop:
			return fmt.Errorf("Docker for desktop is not supported. To create a test cluster to try out Pixie, use minikube instead.  ")
		case ClusterTypeMinikubeOther:
			return fmt.Errorf("Unrecognized minikube driver. Please use kvm2 or HyperKit instead.  ")
		default:
			return nil
		}
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
		cmd := k8s.KubectlCmd("version", "-o", "yaml")
		result, err := cmd.Output()
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
		cmd := k8s.KubectlCmd("auth", "can-i", "create", "namespace")
		result, err := cmd.Output()
		if err != nil {
			return err
		}
		s := string(result)
		if strings.TrimSpace(s) != "yes" {
			return errors.New("user does not have permission to create namespace")
		}
		return nil
	})
	// allowListClusterCheck verifies whether the cluster is in the list of known supported types.
	allowListClusterCheck = NamedCheck("Cluster type is in list of known supported types", func() error {
		clusterType := detectClusterType()
		for _, c := range allowedClusterTypes {
			if c == clusterType {
				return nil
			}
		}

		return errors.New("Cluster type is not in list of known supported cluster types. Please see: https://docs.px.dev/installing-pixie/requirements/")
	})
)

// DefaultClusterChecks is a list of cluster that are performed by default.
var DefaultClusterChecks = []Checker{
	kernelVersionCheck,
	clusterTypeIsSupported,
	k8sVersionCheck,
	hasKubectlCheck,
	userCanCreateNamespace,
}

// ExtraClusterChecks is a list of checks for the cluster that are not required for deployment, but are highly recommended.
var ExtraClusterChecks = []Checker{
	allowListClusterCheck,
}
