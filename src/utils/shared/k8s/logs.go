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
	"archive/zip"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"

	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
)

// LogCollector collect logs for Pixie and cluster setup information.
type LogCollector struct {
	k8sConfig    *rest.Config
	k8sClientSet *kubernetes.Clientset
}

// NewLogCollector creates a new log collector.
func NewLogCollector() *LogCollector {
	cfg := GetConfig()
	cs := GetClientset(cfg)
	return &LogCollector{
		k8sConfig:    cfg,
		k8sClientSet: cs,
	}
}

func fileNameFromParams(ns string, podName string, containerName string, prev bool) string {
	suffix := "log"
	if prev {
		suffix = "prev.log"
	}
	return fmt.Sprintf("%s__%s__%s.%s", ns, podName, containerName, suffix)
}

func (c *LogCollector) logPodInfoToZipFile(zf *zip.Writer, pod v1.Pod, containerName string, prev bool) error {
	fName := fileNameFromParams(pod.Namespace, pod.Name, containerName, prev)
	w, err := zf.Create(fName)
	if err != nil {
		return err
	}
	defer zf.Flush()

	logOpts := &v1.PodLogOptions{
		Container: containerName,
		Previous:  prev,
	}
	req := c.k8sClientSet.CoreV1().Pods(pod.Namespace).GetLogs(pod.Name, logOpts)
	podLogs, err := req.Stream(context.Background())
	if err != nil {
		return err
	}
	defer podLogs.Close()
	_, err = io.Copy(w, podLogs)
	if err != nil {
		return err
	}
	return nil
}

func (c *LogCollector) logKubeCmd(zf *zip.Writer, fName string, arg ...string) error {
	cmd := exec.Command("kubectl", arg...)
	w, err := zf.Create(fName)
	defer zf.Flush()

	if err != nil {
		return err
	}

	stdoutPipe, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}

	err = cmd.Start()
	if err != nil {
		return err
	}
	go func() {
		_, err := io.Copy(w, stdoutPipe)
		if err != nil {
			log.WithError(err).Error("Failed to copy stdout")
		}
	}()
	return cmd.Wait()
}

func (c *LogCollector) writePodDescription(zf *zip.Writer, pod v1.Pod) error {
	w, err := zf.Create(fmt.Sprintf("%s__%s__describe.json", pod.Namespace, pod.Name))
	defer zf.Flush()

	if err != nil {
		return err
	}

	enc := json.NewEncoder(w)
	return enc.Encode(pod)
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

	vls := VizierLabelSelector()
	vizierLabelSelector := metav1.FormatLabelSelector(&vls)

	// We check across all namespaces for the matching pixie pods.
	vizierPodList, err := c.k8sClientSet.CoreV1().Pods("").List(context.Background(), metav1.ListOptions{LabelSelector: vizierLabelSelector})
	if err != nil {
		return err
	}

	// We also need to get the logs the operator logs.
	// As the LabelSelectors are ANDed, we need to make a new query and merge
	// the results.
	ols := OperatorLabelSelector()
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
			_ = c.logPodInfoToZipFile(zf, pod, containerStatus.Name, true)

			err := c.logPodInfoToZipFile(zf, pod, containerStatus.Name, false)
			if err != nil {
				log.WithError(err).Warnf("Failed to log pod: %s", pod.Name)
			}
		}
		err = c.writePodDescription(zf, pod)
		if err != nil {
			log.WithError(err).Warnf("failed to write pod description")
		}
	}

	err = c.logKubeCmd(zf, "nodes.log", "describe", "node")
	if err != nil {
		log.WithError(err).Warn("failed to log node info")
	}

	err = c.logKubeCmd(zf, "services.log", "describe", "services", "--all-namespaces", "-l", vizierLabelSelector)
	if err != nil {
		log.WithError(err).Warnf("failed to log services")
	}

	// Describe vizier and write it to vizier.log
	err = c.logKubeCmd(zf, "vizier.log", "describe", "vizier", "--all-namespaces")
	if err != nil {
		log.WithError(err).Warnf("failed to log vizier crd")
	}

	return nil
}
