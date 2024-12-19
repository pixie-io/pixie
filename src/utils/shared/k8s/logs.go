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
	"fmt"
	"io"
	"os/exec"

	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
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

func (c *LogCollector) LogPodInfoToZipFile(zf *zip.Writer, pod v1.Pod, containerName string, prev bool) error {
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

func (c *LogCollector) LogKubeCmd(zf *zip.Writer, fName string, arg ...string) error {
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

func (c *LogCollector) WritePodDescription(zf *zip.Writer, pod v1.Pod) error {
	w, err := zf.Create(fmt.Sprintf("%s__%s__describe.json", pod.Namespace, pod.Name))
	defer zf.Flush()

	if err != nil {
		return err
	}

	enc := json.NewEncoder(w)
	return enc.Encode(pod)
}

func (c *LogCollector) LogOutputToZipFile(zf *zip.Writer, fName string, output string) error {
	w, err := zf.Create(fName)
	if err != nil {
		return err
	}
	defer zf.Flush()

	_, err = w.Write([]byte(output))
	return err
}
