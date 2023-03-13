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

package k8s

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/spf13/pflag"
	"k8s.io/client-go/discovery"
	"k8s.io/client-go/kubernetes"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	clientcmdapi "k8s.io/client-go/tools/clientcmd/api"
)

// Contents in this file are copied and modified from
// https://github.com/kubernetes/client-go/blob/master/examples/out-of-cluster-client-configuration/main.go

var kubeconfig *string

// fileExists checks if a file exists and is not a directory before we
// try using it to prevent further errors.
func fileExists(filename string) bool {
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return false
	}
	return !info.IsDir()
}

func init() {
	defaultKubeConfig := ""
	optionalStr := "(optional) "
	if k := os.Getenv("KUBECONFIG"); k != "" {
		for _, config := range strings.Split(k, ":") {
			if fileExists(config) {
				defaultKubeConfig = config
				break
			}
		}
		if defaultKubeConfig == "" {
			// Don't use log.Fatal, because it will send an error to Sentry when invoked from the CLI.
			fmt.Println("Failed to find valid config in KUBECONFIG env. Is it formatted correctly?")
			os.Exit(1)
		}
	} else if home := homeDir(); home != "" {
		defaultKubeConfig = filepath.Join(home, ".kube", "config")
	} else {
		optionalStr = ""
	}

	kubeconfig = pflag.String("kubeconfig", defaultKubeConfig, fmt.Sprintf("%sabsolute path to the kubeconfig file", optionalStr))
}

// GetClientset gets the clientset for the current kubernetes cluster.
func GetClientset(config *rest.Config) *kubernetes.Clientset {
	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		// Don't use log.Fatal, because it will send an error to Sentry when invoked from the CLI.
		fmt.Printf("Could not create k8s clientset: %s\n", err.Error())
		os.Exit(1)
	}
	return clientset
}

// GetDiscoveryClient gets the discovery client for the current kubernetes cluster.
func GetDiscoveryClient(config *rest.Config) *discovery.DiscoveryClient {
	discoveryClient, err := discovery.NewDiscoveryClientForConfig(config)
	if err != nil {
		// Don't use log.Fatal, because it will send an error to Sentry when invoked from the CLI.
		fmt.Printf("Could not create k8s discovery client: %s\n", err.Error())
		os.Exit(1)
	}

	return discoveryClient
}

// GetConfig gets the kubernetes rest config.
func GetConfig() *rest.Config {
	// use the current context in kubeconfig
	config, err := clientcmd.BuildConfigFromFlags("", *kubeconfig)
	if err != nil {
		// Don't use log.Fatal, because it will send an error to Sentry when invoked from the CLI.
		fmt.Printf("Could not build kubeconfig: %s\n", err.Error())
		os.Exit(1)
	}

	return config
}

// GetClientAPIConfig gets the config used for reading the current kube contexts.
func GetClientAPIConfig() *clientcmdapi.Config {
	return clientcmd.GetConfigFromFileOrDie(*kubeconfig)
}

func GetKubeconfigPath() string {
	return *kubeconfig
}

func homeDir() string {
	if h := os.Getenv("HOME"); h != "" {
		return h
	}
	return os.Getenv("USERPROFILE") // windows
}
