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

package steps

import (
	"bytes"
	"context"
	"errors"
	"io"
	"os"
	"os/exec"
	"path"
	"text/template"

	"github.com/Masterminds/sprig/v3"
	"gopkg.in/yaml.v3"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/utils/shared/k8s"
)

// DeployStep is the interface for running a stage in a given workloads deploy process.
type DeployStep interface {
	// Prepare runs any preparation steps that don't require an assigned cluster.
	Prepare() error
	// Deploy runs the deploy step, and returns any namespaces that should be deleted on Close.
	Deploy(*cluster.Context) ([]string, error)
	// Name returns a printable name for the deploy step.
	Name() string
}

type renderedYAML struct {
	namespace string
	yaml      []byte
}

func (r *renderedYAML) deploy(clusterCtx *cluster.Context) error {
	ns, err := r.getNamespace()
	if err != nil {
		return err
	}
	if err := createNamespaceIfNotExists(clusterCtx, ns); err != nil {
		return err
	}
	return k8s.ApplyYAML(
		clusterCtx.Clientset(),
		clusterCtx.RestConfig(),
		ns,
		bytes.NewReader(r.yaml),
		true,
	)
}

const kustomizeForPatchesTmpl = `
---
apiVersion: kustomize.config.k8s.io/v1beta1
kind: Kustomization
resources:
- resources.yaml
patches:
{{range $p := .}}
- patch: |-
{{ indent 4 $p.YAML }}
  target:
    {{ if ne $p.Target.APIGroup "" }}group: {{ $p.Target.APIGroup }}{{end}}
    {{ if ne $p.Target.APIVersion "" }}version: {{ $p.Target.APIVersion }}{{end}}
    {{ if ne $p.Target.Kind "" }}kind: {{ $p.Target.Kind }}{{end}}
    {{ if ne $p.Target.Name "" }}name: {{ $p.Target.Name }}{{end}}
    {{ if ne $p.Target.Namespace "" }}namespace: {{ $p.Target.Namespace }}{{end}}
    {{ if ne $p.Target.LabelSelector "" }}labelSelector: "{{ $p.Target.LabelSelector }}"{{end}}
    {{ if ne $p.Target.AnnotationSelector "" }}annotationSelector: "{{ $p.Target.AnnotationSelect }}"{{end}}
{{- end -}}
`

func (r *renderedYAML) patch(patches []*experimentpb.PatchSpec) error {
	if len(patches) == 0 {
		return nil
	}

	tmpdir, err := os.MkdirTemp("", "*")
	if err != nil {
		return err
	}
	defer os.RemoveAll(tmpdir)

	if err := os.WriteFile(path.Join(tmpdir, "resources.yaml"), r.yaml, 0666); err != nil {
		return err
	}

	f, err := os.Create(path.Join(tmpdir, "kustomization.yaml"))
	if err != nil {
		return err
	}
	defer f.Close()
	t, err := template.New("").Funcs(sprig.TxtFuncMap()).Parse(kustomizeForPatchesTmpl)
	if err != nil {
		return err
	}
	if err := t.Execute(f, patches); err != nil {
		return err
	}

	cmd := exec.Command("kustomize", "build", tmpdir)
	out := &bytes.Buffer{}
	cmd.Stdout = out
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return err
	}
	r.yaml = out.Bytes()
	return nil
}

type k8sYAML struct {
	Metadata *metadata
}
type metadata struct {
	Namespace string
}

func (r *renderedYAML) getNamespace() (string, error) {
	if r.namespace != "" {
		return r.namespace, nil
	}

	namespaces := make(map[string]bool)
	namespace := ""

	d := yaml.NewDecoder(bytes.NewReader(r.yaml))
	for {
		var y k8sYAML
		if err := d.Decode(&y); err != nil {
			if err == io.EOF {
				break
			}
			return "", err
		}
		if y.Metadata.Namespace != "" {
			namespaces[y.Metadata.Namespace] = true
			namespace = y.Metadata.Namespace
		}
	}

	if len(namespaces) > 1 {
		return "", errors.New("Rendered YAML deploys (skaffold or prerendered) only support a single namespace per DeployStep")
	}

	return namespace, nil
}

func createNamespaceIfNotExists(clusterCtx *cluster.Context, namespace string) error {
	_, err := clusterCtx.Clientset().CoreV1().Namespaces().Get(context.Background(), namespace, metav1.GetOptions{})
	if err == nil {
		// The namespace already exists skip creating it.
		return nil
	}
	ns := &v1.Namespace{
		ObjectMeta: metav1.ObjectMeta{
			Name: namespace,
		},
	}
	_, err = clusterCtx.Clientset().CoreV1().Namespaces().Create(context.Background(), ns, metav1.CreateOptions{})
	return err
}
