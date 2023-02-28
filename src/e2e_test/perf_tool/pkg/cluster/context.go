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

package cluster

import (
	"fmt"
	"os"
	"os/exec"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"

	"px.dev/pixie/src/utils/shared/k8s"
)

// Context stores the config and clients to connect with a particular kubernetes cluster.
type Context struct {
	configPath string
	restConfig *rest.Config
	clientset  *kubernetes.Clientset

	tmpFilePath string
}

// NewContextFromPath creates a new Context for the cluster specified in the given kubeconfig file.
func NewContextFromPath(kubeconfigPath string) (*Context, error) {
	restConfig, err := clientcmd.BuildConfigFromFlags("", kubeconfigPath)
	if err != nil {
		return nil, err
	}
	clientset := k8s.GetClientset(restConfig)
	return &Context{
		configPath: kubeconfigPath,
		restConfig: restConfig,
		clientset:  clientset,
	}, nil
}

// NewContextFromConfig writes the given kubeconfig to a file, and the returns NewContextFromPath for that file.
func NewContextFromConfig(kubeconfig []byte) (*Context, error) {
	tmpFile, err := os.CreateTemp("", "*")
	if err != nil {
		return nil, err
	}
	defer tmpFile.Close()
	if _, err := tmpFile.Write(kubeconfig); err != nil {
		return nil, err
	}
	ctx, err := NewContextFromPath(tmpFile.Name())
	if err != nil {
		return nil, err
	}
	ctx.tmpFilePath = tmpFile.Name()
	return ctx, nil
}

// Close the context, removing the temp file if it exists.
func (ctx *Context) Close() error {
	if ctx.tmpFilePath != "" {
		return os.Remove(ctx.tmpFilePath)
	}
	return nil
}

// AddEnv adds the necessary environment variables to a exec.Cmd,
// such that the command will use the k8s cluster specified by this context.
func (ctx *Context) AddEnv(cmd *exec.Cmd) {
	cmd.Env = append(cmd.Environ(), fmt.Sprintf("KUBECONFIG=%s", ctx.configPath))
}

// Clientset returns the kubernetes Clientset for this cluster.
func (ctx *Context) Clientset() *kubernetes.Clientset {
	return ctx.clientset
}

// RestConfig returns the kubernetes client config for this cluster.
func (ctx *Context) RestConfig() *rest.Config {
	return ctx.restConfig
}
