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

package vizier

import (
	"archive/zip"
	"context"
	"errors"
	"os"
	"strings"

	log "github.com/sirupsen/logrus"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/utils/script"
	"px.dev/pixie/src/utils/shared/k8s"
)

// LogCollector collect logs for Pixie and cluster setup information.
type LogCollector struct {
	k8sConfig    *rest.Config
	k8sClientSet *kubernetes.Clientset
	cloudAddr    string
	br           *script.BundleManager
	k8s.LogCollector
}

// NewLogCollector creates a new log collector.
func NewLogCollector(br *script.BundleManager, cloudAddr string) *LogCollector {
	cfg := k8s.GetConfig()
	cs := k8s.GetClientset(cfg)
	return &LogCollector{
		cfg,
		cs,
		cloudAddr,
		br,
		*k8s.NewLogCollector(),
	}
}

// CollectPixieLogs collects logs for all Pixie pods and write them to the zip file fName.
func (c *LogCollector) CollectPixieLogs(fName string) error {
	if !strings.HasSuffix(fName, ".zip") {
		return errors.New("fname must have .zip suffix")
	}
	f, err := os.Create(fName)
	if err != nil {
		return err
	}
	defer f.Close()

	zf := zip.NewWriter(f)
	defer zf.Close()

	vls := k8s.VizierLabelSelector()
	vizierLabelSelector := metav1.FormatLabelSelector(&vls)

	// We check across all namespaces for the matching pixie pods.
	vizierPodList, err := c.k8sClientSet.CoreV1().Pods("").List(context.Background(), metav1.ListOptions{LabelSelector: vizierLabelSelector})
	if err != nil {
		return err
	}

	// We also need to get the logs the operator logs.
	// As the LabelSelectors are ANDed, we need to make a new query and merge
	// the results.
	ols := k8s.OperatorLabelSelector()
	operatorLabelSelector := metav1.FormatLabelSelector(&ols)

	operatorPodList, err := c.k8sClientSet.CoreV1().Pods("").List(context.Background(), metav1.ListOptions{LabelSelector: operatorLabelSelector})
	if err != nil {
		return err
	}

	// Merge the two pod lists
	pods := append(vizierPodList.Items, operatorPodList.Items...)

	for _, pod := range pods {
		for _, containerStatus := range pod.Status.ContainerStatuses {
			// Ignore prev logs, they might not exist.
			_ = c.LogPodInfoToZipFile(zf, pod, containerStatus.Name, true)

			err := c.LogPodInfoToZipFile(zf, pod, containerStatus.Name, false)
			if err != nil {
				log.WithError(err).Warnf("Failed to log pod: %s", pod.Name)
			}
		}
		err = c.WritePodDescription(zf, pod)
		if err != nil {
			log.WithError(err).Warnf("failed to write pod description")
		}
	}

	err = c.LogKubeCmd(zf, "nodes.log", "describe", "node")
	if err != nil {
		log.WithError(err).Warn("failed to log node info")
	}

	err = c.LogKubeCmd(zf, "services.log", "describe", "services", "--all-namespaces", "-l", vizierLabelSelector)
	if err != nil {
		log.WithError(err).Warnf("failed to log services")
	}

	// Describe vizier and write it to vizier.log
	err = c.LogKubeCmd(zf, "vizier.log", "describe", "vizier", "--all-namespaces")
	if err != nil {
		log.WithError(err).Warnf("failed to log vizier crd")
	}

	clusterID, err := GetCurrentVizier(c.cloudAddr)
	if err != nil {
		log.WithError(err).Warnf("failed to get cluster ID")
	}
	outputCh, err := RunSimpleHealthCheckScript(c.br, c.cloudAddr, clusterID)

	if err != nil {
		entry := log.WithError(err)
		if _, ok := err.(*HealthCheckWarning); ok {
			entry.Warn("healthcheck script detected the following warnings:")
		} else {
			entry.Warn("failed to run healthcheck script")
		}
	}

	return c.LogOutputToZipFile(zf, "px_agent_diagnostics.txt", <-outputCh)
}
