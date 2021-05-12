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

package vizieryamls

import (
	"fmt"
	"os"
	"strconv"
	"strings"

	"k8s.io/client-go/kubernetes"

	"px.dev/pixie/src/utils/shared/tar"
	"px.dev/pixie/src/utils/shared/yamls"
)

const (
	etcdOperatorYAMLPath          = "./yamls/vizier_deps/etcd_operator_prod.yaml"
	vizierEtcdYAMLPath            = "./yamls/vizier/vizier_etcd_metadata_prod.yaml"
	vizierMetadataPersistYAMLPath = "./yamls/vizier/vizier_metadata_persist_prod.yaml"
	natsYAMLPath                  = "./yamls/vizier_deps/nats_prod.yaml"
	defaultMemoryLimit            = "2Gi"
)

// Sentry configs are not actually secret and safe to check in.
const (
	// We can't really distinguish between prod/dev, so we use some heuristics to decide.
	prodSentryDSN = "https://a8a635734bb840799befb63190e904e0@o324879.ingest.sentry.io/5203506"
	devSentryDSN  = "https://8e4acf22871543f1aa143a93a5216a16@o324879.ingest.sentry.io/5203508"
)

// VizierTmplValues are the template values that can be used to fill out templated Vizier YAMLs.
type VizierTmplValues struct {
	DeployKey         string
	CustomAnnotations string
	CustomLabels      string
	CloudAddr         string
	ClusterName       string
	CloudUpdateAddr   string
	UseEtcdOperator   bool
	BootstrapVersion  string
	PEMMemoryLimit    string
	Namespace         string
}

// VizierTmplValuesToArgs converts the vizier template values to args which can be used to fill out a template.
func VizierTmplValuesToArgs(tmplValues *VizierTmplValues) *yamls.YAMLTmplArguments {
	return &yamls.YAMLTmplArguments{
		Values: &map[string]interface{}{
			"deployKey":         tmplValues.DeployKey,
			"customAnnotations": tmplValues.CustomAnnotations,
			"customLabels":      tmplValues.CustomLabels,
			"cloudAddr":         tmplValues.CloudAddr,
			"clusterName":       tmplValues.ClusterName,
			"cloudUpdateAddr":   tmplValues.CloudUpdateAddr,
			"useEtcdOperator":   tmplValues.UseEtcdOperator,
			"bootstrapVersion":  tmplValues.BootstrapVersion,
			"pemMemoryLimit":    tmplValues.PEMMemoryLimit,
		},
		Release: &map[string]interface{}{
			"Namespace": tmplValues.Namespace,
		},
	}
}

var nsTmpl = `{{ if .Release.Namespace }}{{ .Release.Namespace }}{{ else }}pl{{ end }}`

// These are template options that should be applied to each resource in the Vizier YAMLs, such as annotations and labels.
var GlobalTemplateOptions = []*yamls.K8sTemplateOptions{
	{
		Patch:       `{"metadata": { "annotations": { "__PL_ANNOTATION_KEY__": "__PL_ANNOTATION_VALUE__"} } }`,
		Placeholder: "__PL_ANNOTATION_KEY__: __PL_ANNOTATION_VALUE__",
		TemplateValue: `{{if .Values.customAnnotations}}{{range $element := split "," .Values.customAnnotations -}}
    {{ $kv := split "=" $element -}}
    {{if eq (len $kv) 2 -}}
    {{ $kv._0 }}: "{{ $kv._1 }}"
    {{- end}}
    {{end}}{{end}}`,
	},
	{
		TemplateMatcher: yamls.TemplateScopeMatcher,
		Patch:           `{"spec": { "template": { "metadata": { "annotations": { "__PL_SPEC_ANNOTATION_KEY__": "__PL_SPEC_ANNOTATION_VALUE__"} } } } }`,
		Placeholder:     "__PL_SPEC_ANNOTATION_KEY__: __PL_SPEC_ANNOTATION_VALUE__",
		TemplateValue: `{{if .Values.customAnnotations}}{{range $element := split "," .Values.customAnnotations -}}
        {{ $kv := split "=" $element -}}
        {{if eq (len $kv) 2 -}}
        {{ $kv._0 }}: "{{ $kv._1 }}"
        {{- end}}
        {{end}}{{end}}`,
	},
	{
		Patch:       `{"metadata": { "labels": { "__PL_LABEL_KEY__": "__PL_LABEL_VALUE__"} } }`,
		Placeholder: "__PL_LABEL_KEY__: __PL_LABEL_VALUE__",
		TemplateValue: `{{if .Values.customLabels}}{{range $element := split "," .Values.customLabels -}}
    {{ $kv := split "=" $element -}}
    {{if eq (len $kv) 2 -}}
    {{ $kv._0 }}: "{{ $kv._1 }}"
    {{- end}}
    {{end}}{{end}}`,
	},
	{
		TemplateMatcher: yamls.TemplateScopeMatcher,
		Patch:           `{"spec": { "template": { "metadata": { "labels": { "__PL_SPEC_LABEL_KEY__": "__PL_SPEC_LABEL_VALUE__"} } } } }`,
		Placeholder:     "__PL_SPEC_LABEL_KEY__: __PL_SPEC_LABEL_VALUE__",
		TemplateValue: `{{if .Values.customLabels}}{{range $element := split "," .Values.customLabels -}}
        {{ $kv := split "=" $element -}}
        {{if eq (len $kv) 2 -}}
        {{ $kv._0 }}: "{{ $kv._1 }}"
        {{- end}}
        {{end}}{{end}}`,
	},
	{
		Patch:         `{"metadata": { "namespace": "__PX_NAMESPACE__" } }`,
		Placeholder:   "__PX_NAMESPACE__",
		TemplateValue: nsTmpl,
	},
	{
		TemplateMatcher: yamls.GenerateServiceAccountSubjectMatcher("default"),
		Patch:           `{ "subjects": [{ "name": "default", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
		Placeholder:     "__PX_SUBJECT_NAMESPACE__",
		TemplateValue:   nsTmpl,
	},
}

func getSentryDSN(vizierVersion string) string {
	// If it contains - it must be a pre-release Vizier.
	if strings.Contains(vizierVersion, "-") {
		return devSentryDSN
	}
	return prodSentryDSN
}

// GenerateTemplatedDeployYAMLsWithTar generates the YAMLs that should be run when deploying Pixie using the provided tar file.
func GenerateTemplatedDeployYAMLsWithTar(clientset *kubernetes.Clientset, tarPath string, versionStr string) ([]*yamls.YAMLFile, error) {
	file, err := os.Open(tarPath)
	if err != nil {
		return nil, err
	}

	yamlMap, err := tar.ReadTarFileFromReader(file)
	if err != nil {
		return nil, err
	}

	return GenerateTemplatedDeployYAMLs(clientset, yamlMap, versionStr)
}

// GenerateTemplatedDeployYAMLs generates the YAMLs that should be run when deploying Pixie using the provided YAML map.
func GenerateTemplatedDeployYAMLs(clientset *kubernetes.Clientset, yamlMap map[string]string, versionStr string) ([]*yamls.YAMLFile, error) {
	secretsYAML, err := GenerateSecretsYAML(clientset, versionStr, false)
	if err != nil {
		return nil, err
	}

	natsYAML, etcdYAML, err := generateVzDepsYAMLs(clientset, yamlMap)
	if err != nil {
		return nil, err
	}

	etcdVzYAML, persistentVzYAML, err := generateVzYAMLs(clientset, yamlMap)
	if err != nil {
		return nil, err
	}

	return []*yamls.YAMLFile{
		{
			Name: "secrets",
			YAML: secretsYAML,
		},
		{
			Name: "nats",
			YAML: natsYAML,
		},
		{
			Name: "etcd",
			YAML: etcdYAML,
		},
		{
			Name: "vizier_etcd",
			YAML: etcdVzYAML,
		},
		{
			Name: "vizier_persistent",
			YAML: persistentVzYAML,
		},
	}, nil
}

// GenerateSecretsYAML creates the YAML for Pixie secrets.
func GenerateSecretsYAML(clientset *kubernetes.Clientset, versionStr string, bootstrapMode bool) (string, error) {
	dockerYAML := ""

	csYAMLs, err := generateClusterSecretYAMLs(getSentryDSN(versionStr), bootstrapMode)
	if err != nil {
		return "", err
	}

	origYAML := yamls.ConcatYAMLs(dockerYAML, csYAMLs)

	// Fill in configmaps.
	secretsYAML, err := yamls.TemplatizeK8sYAML(clientset, origYAML, append([]*yamls.K8sTemplateOptions{
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-deploy-secrets"),
			Patch:           `{"stringData": { "deploy-key": "__PL_DEPLOY_KEY__"} }`,
			Placeholder:     "__PL_DEPLOY_KEY__",
			TemplateValue:   `"{{ .Values.deployKey }}"`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_CUSTOM_ANNOTATIONS": "__PL_CUSTOM_ANNOTATIONS__"} }`,
			Placeholder:     "__PL_CUSTOM_ANNOTATIONS__",
			TemplateValue:   `"{{ .Values.customAnnotations }}"`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_CUSTOM_LABELS": "__PL_CUSTOM_LABELS__"} }`,
			Placeholder:     "__PL_CUSTOM_LABELS__",
			TemplateValue:   `"{{ .Values.customLabels }}"`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PX_MEMORY_LIMIT": "__PX_MEMORY_LIMIT__"} }`,
			Placeholder:     "__PX_MEMORY_LIMIT__",
			TemplateValue:   `"{{ .Values.pemMemoryLimit }}"`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-config"),
			Patch:           `{"data": { "PL_CLOUD_ADDR": "__PL_CLOUD_ADDR__"} }`,
			Placeholder:     "__PL_CLOUD_ADDR__",
			TemplateValue:   `{{ if .Values.cloudAddr }}"{{ .Values.cloudAddr }}"{{ else }}"withpixie.ai:443"{{ end }}`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-config"),
			Patch:           `{"data": { "PL_UPDATE_CLOUD_ADDR": "__PL_UPDATE_CLOUD_ADDR__"} }`,
			Placeholder:     "__PL_UPDATE_CLOUD_ADDR__",
			TemplateValue:   `{{ if .Values.cloudUpdateAddr }}"{{ .Values.cloudUpdateAddr }}"{{ else }}"withpixie.ai:443"{{ end }}`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-config"),
			Patch:           `{"data": { "PL_CLUSTER_NAME": "__PL_CLUSTER_NAME__"} }`,
			Placeholder:     "__PL_CLUSTER_NAME__",
			TemplateValue:   `"{{ .Values.clusterName }}"`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_ETCD_OPERATOR_ENABLED": "__PL_ETCD_OPERATOR_ENABLED__"} }`,
			Placeholder:     "__PL_ETCD_OPERATOR_ENABLED__",
			TemplateValue:   `{{ if .Values.useEtcdOperator }}"true"{{else}}"false"{{end}}`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_MD_ETCD_SERVER": "__PL_MD_ETCD_SERVER__"} }`,
			Placeholder:     "__PL_MD_ETCD_SERVER__",
			TemplateValue:   fmt.Sprintf(`{{ if .Values.useEtcdOperator }}"https://pl-etcd-client.%s.svc:2379"{{else}}"https://etcd.%s.svc:2379"{{end}}`, nsTmpl, nsTmpl),
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-connector-bootstrap-config"),
			Patch:           `{"data": { "PL_BOOTSTRAP_VERSION": "__PL_BOOTSTRAP_VERSION__"} }`,
			Placeholder:     "__PL_BOOTSTRAP_VERSION__",
			TemplateValue:   `"{{.Values.bootstrapVersion}}"`,
		},
	}, GlobalTemplateOptions...))
	if err != nil {
		return "", err
	}

	return secretsYAML, nil
}

func generateVzDepsYAMLs(clientset *kubernetes.Clientset, yamlMap map[string]string) (string, string, error) {
	natsYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[natsYAMLPath], append(GlobalTemplateOptions, []*yamls.K8sTemplateOptions{
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl:nats-operator-binding"),
			Patch:           `{ "subjects": [{ "name": "nats-operator", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl:nats-server-binding"),
			Patch:           `{ "subjects": [{ "name": "nats-server", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
	}...))
	if err != nil {
		return "", "", err
	}

	etcdYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[etcdOperatorYAMLPath], GlobalTemplateOptions)
	if err != nil {
		return "", "", err
	}
	// The etcdYAML should only be applied if --use_etcd_operator is true. The entire YAML should be wrapped in a template.
	wrappedEtcd := fmt.Sprintf(
		`{{if .Values.useEtcdOperator}}
%s
{{- end}}`,
		etcdYAML)

	return natsYAML, wrappedEtcd, nil
}

func generateVzYAMLs(clientset *kubernetes.Clientset, yamlMap map[string]string) (string, string, error) {
	if _, ok := yamlMap[vizierMetadataPersistYAMLPath]; !ok {
		return "", "", fmt.Errorf("Cannot generate YAMLS for specified Vizier version. Please update to latest Vizier version instead.  ")
	}

	tmplOptions := append(GlobalTemplateOptions, []*yamls.K8sTemplateOptions{
		{
			TemplateMatcher: yamls.GenerateContainerNameMatcherFn("pem"),
			Patch:           `{"spec": { "template": { "spec": { "containers": [{ "name": "pem", "resources": { "limits": { "memory": "__PX_MEMORY_LIMIT__"} } }] } } } }`,
			Placeholder:     "__PX_MEMORY_LIMIT__",
			TemplateValue:   fmt.Sprintf(`{{ if .Values.pemMemoryLimit }}"{{ .Values.pemMemoryLimit }}"{{else}}"%s"{{end}}`, defaultMemoryLimit),
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("proxy-envoy-config"),
			Patch:           `{}`,
			Placeholder:     ".pl.svc",
			TemplateValue:   fmt.Sprintf(".%s.svc", nsTmpl),
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-psp-binding"),
			Patch:           `{ "subjects": [{ "name": "updater-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-updater-binding"),
			Patch:           `{ "subjects": [{ "name": "updater-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-connector-binding"),
			Patch:           `{ "subjects": [{ "name": "cloud-conn-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
	}...)

	persistentYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[vizierMetadataPersistYAMLPath], tmplOptions)

	if err != nil {
		return "", "", err
	}
	// The persistent YAML should only be applied if --use_etcd_operator is false. The entire YAML should be wrapped in a template.
	wrappedPersistent := fmt.Sprintf(
		`{{if not .Values.useEtcdOperator}}
%s
{{- end}}`,
		persistentYAML)

	etcdYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[vizierEtcdYAMLPath], tmplOptions)
	if err != nil {
		return "", "", err
	}
	// The etcd version of Vizier should only be applied if --use_etcd_operator is true. The entire YAML should be wrapped in a template.
	wrappedEtcd := fmt.Sprintf(
		`{{if .Values.useEtcdOperator}}
%s
{{- end}}`,
		etcdYAML)

	return wrappedEtcd, wrappedPersistent, nil
}

const secretsYAML string = `
apiVersion: v1
kind: ConfigMap
metadata:
  creationTimestamp: null
  name: pl-cloud-config
  namespace: pl
---
apiVersion: v1
kind: ConfigMap
metadata:
  creationTimestamp: null
  name: pl-cluster-config
  namespace: pl
---
apiVersion: v1
stringData:
  sentry-dsn: __PL_SENTRY_DSN__
kind: Secret
metadata:
  creationTimestamp: null
  name: pl-cluster-secrets
  namespace: pl
---
apiVersion: v1
data:
  PL_BOOTSTRAP_MODE: "__PL_BOOTSTRAP_MODE__"
kind: ConfigMap
metadata:
  creationTimestamp: null
  labels:
    component: vizier
  name: pl-cloud-connector-bootstrap-config
  namespace: pl
---
apiVersion: v1
kind: Secret
metadata:
  creationTimestamp: null
  name: pl-deploy-secrets
  namespace: pl
`

func generateClusterSecretYAMLs(sentryDSN string, bootstrapMode bool) (string, error) {
	// Perform substitutions.
	r := strings.NewReplacer(
		"__PL_SENTRY_DSN__", sentryDSN,
		"__PL_BOOTSTRAP_MODE__", strconv.FormatBool(bootstrapMode),
	)
	return r.Replace(secretsYAML), nil
}
