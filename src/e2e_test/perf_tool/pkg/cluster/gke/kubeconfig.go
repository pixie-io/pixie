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

package gke

import (
	"bytes"
	"text/template"
)

const kubeconfigTempl = `
apiVersion: v1
kind: Config
preferences: {}
clusters:
- cluster:
    certificate-authority-data: {{.ClusterCA}}
    server: {{.Endpoint}}
  name: {{.ClusterName}}
contexts:
- context:
    cluster: {{.ClusterName}}
    user: {{.ClusterName}}
  name: {{.ClusterName}}
current-context: {{.ClusterName}}
users:
- name: {{.ClusterName}}
  user:
    exec:
      apiVersion: client.authentication.k8s.io/v1beta1
      command: gke-gcloud-auth-plugin
      args:
      - --use_application_default_credentials
      installHint: Install gke-gcloud-auth-plugin for use with kubectl by following
        https://cloud.google.com/blog/products/containers-kubernetes/kubectl-auth-changes-in-gke
      provideClusterInfo: true
`

func fillKubeconfigTemplate(name string, ca string, endpoint string) ([]byte, error) {
	t, err := template.New("").Parse(kubeconfigTempl)
	if err != nil {
		return nil, err
	}
	var buf bytes.Buffer
	err = t.Execute(&buf, &struct {
		ClusterName string
		ClusterCA   string
		Endpoint    string
	}{
		ClusterName: name,
		ClusterCA:   ca,
		Endpoint:    endpoint,
	})
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}
