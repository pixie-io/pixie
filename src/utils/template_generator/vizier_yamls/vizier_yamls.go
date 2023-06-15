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
	"strings"

	"px.dev/pixie/src/utils/shared/tar"
	"px.dev/pixie/src/utils/shared/yamls"
)

const (
	vizierEtcdYAMLPath                     = "yamls/vizier/vizier_etcd_metadata_prod.yaml"
	vizierMetadataPersistYAMLPath          = "yamls/vizier/vizier_metadata_persist_prod.yaml"
	vizierEtcdAutopilotYAMLPath            = "yamls/vizier/vizier_etcd_metadata_autopilot_prod.yaml"
	vizierMetadataPersistAutopilotYAMLPath = "yamls/vizier/vizier_metadata_persist_autopilot_prod.yaml"
	etcdYAMLPath                           = "yamls/vizier_deps/etcd_prod.yaml"
	natsYAMLPath                           = "yamls/vizier_deps/nats_prod.yaml"
	// Note: if you update this value, make sure you also update defaultUncappedTableStoreSizeMB in
	// src/cloud/config_manager/controllers/server.go, because we want to make
	// sure that the table store size is about 60% of the total requested memory.
	defaultMemoryLimit      = "2Gi"
	defaultDataAccess       = "Full"
	defaultElectionPeriodMs = 7500
)

// VizierTmplValues are the template values that can be used to fill out templated Vizier YAMLs.
type VizierTmplValues struct {
	DeployKey                 string
	CustomDeployKeySecret     string
	CustomAnnotations         string
	CustomLabels              string
	CloudAddr                 string
	ClusterName               string
	CloudUpdateAddr           string
	UseEtcdOperator           bool
	PEMMemoryLimit            string
	PEMMemoryRequest          string
	Namespace                 string
	DisableAutoUpdate         bool
	SentryDSN                 string
	ClockConverter            string
	DataAccess                string
	DatastreamBufferSize      uint32
	DatastreamBufferSpikeSize uint32
	ElectionPeriodMs          int64
	CustomPEMFlags            map[string]string
	Registry                  string
	UseBetaPdbVersion         bool
}

// VizierTmplValuesToArgs converts the vizier template values to args which can be used to fill out a template.
func VizierTmplValuesToArgs(tmplValues *VizierTmplValues) *yamls.YAMLTmplArguments {
	return &yamls.YAMLTmplArguments{
		Values: &map[string]interface{}{
			"deployKey":                 tmplValues.DeployKey,
			"customDeployKeySecret":     tmplValues.CustomDeployKeySecret,
			"customAnnotations":         tmplValues.CustomAnnotations,
			"customLabels":              tmplValues.CustomLabels,
			"cloudAddr":                 tmplValues.CloudAddr,
			"clusterName":               tmplValues.ClusterName,
			"cloudUpdateAddr":           tmplValues.CloudUpdateAddr,
			"useEtcdOperator":           tmplValues.UseEtcdOperator,
			"pemMemoryLimit":            tmplValues.PEMMemoryLimit,
			"pemMemoryRequest":          tmplValues.PEMMemoryRequest,
			"disableAutoUpdate":         tmplValues.DisableAutoUpdate,
			"sentryDSN":                 tmplValues.SentryDSN,
			"clockConverter":            tmplValues.ClockConverter,
			"dataAccess":                tmplValues.DataAccess,
			"datastreamBufferSize":      tmplValues.DatastreamBufferSize,
			"datastreamBufferSpikeSize": tmplValues.DatastreamBufferSpikeSize,
			"electionPeriodMs":          tmplValues.ElectionPeriodMs,
			"customPEMFlags":            tmplValues.CustomPEMFlags,
			"registry":                  tmplValues.Registry,
			"useBetaPdbVersion":         tmplValues.UseBetaPdbVersion,
		},
		Release: &map[string]interface{}{
			"Namespace": tmplValues.Namespace,
		},
	}
}

var nsTmpl = `{{ if .Release.Namespace }}{{ .Release.Namespace }}{{ else }}pl{{ end }}`

// GlobalTemplateOptions are template options that should be applied to each resource in the Vizier YAMLs, such as annotations and labels.
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

// GenerateTemplatedDeployYAMLsWithTar generates the YAMLs that should be run when deploying Pixie using the provided tar file.
func GenerateTemplatedDeployYAMLsWithTar(tarPath string, versionStr string) ([]*yamls.YAMLFile, error) {
	file, err := os.Open(tarPath)
	if err != nil {
		return nil, err
	}

	yamlMap, err := tar.ReadTarFileFromReader(file)
	if err != nil {
		return nil, err
	}

	return GenerateTemplatedDeployYAMLs(yamlMap, versionStr)
}

// GenerateTemplatedDeployYAMLs generates the YAMLs that should be run when deploying Pixie using the provided YAML map.
func GenerateTemplatedDeployYAMLs(yamlMap map[string]string, versionStr string) ([]*yamls.YAMLFile, error) {
	secretsYAML, err := GenerateSecretsYAML(versionStr)
	if err != nil {
		return nil, err
	}

	natsYAML, etcdYAML, err := generateVzDepsYAMLs(yamlMap)
	if err != nil {
		return nil, err
	}

	vzYAMLs, err := generateVzYAMLs(yamlMap)
	if err != nil {
		return nil, err
	}

	return append([]*yamls.YAMLFile{
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
	}, vzYAMLs...), nil
}

// GenerateSecretsYAML creates the YAML for Pixie secrets.
func GenerateSecretsYAML(versionStr string) (string, error) {
	dockerYAML := ""

	origYAML := yamls.ConcatYAMLs(dockerYAML, secretsYAML)

	// Fill in configmaps.
	secretsYAML, err := yamls.TemplatizeK8sYAML(origYAML, append([]*yamls.K8sTemplateOptions{
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-deploy-secrets"),
			Patch:           `{"stringData": { "deploy-key": "__PL_DEPLOY_KEY__"} }`,
			Placeholder:     "__PL_DEPLOY_KEY__",
			TemplateValue:   `"{{ .Values.deployKey }}"`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-secrets"),
			Patch:           `{"stringData": { "sentry-dsn": "__PL_SENTRY_DSN__"} }`,
			Placeholder:     "__PL_SENTRY_DSN__",
			TemplateValue:   `"{{ .Values.sentryDSN }}"`,
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
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PX_MEMORY_REQUEST": "__PX_MEMORY_REQUEST__"} }`,
			Placeholder:     "__PX_MEMORY_REQUEST__",
			TemplateValue:   `"{{ .Values.pemMemoryRequest }}"`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_DISABLE_AUTO_UPDATE": "__PL_DISABLE_AUTO_UPDATE__"} }`,
			Placeholder:     "__PL_DISABLE_AUTO_UPDATE__",
			TemplateValue:   `{{ if .Values.disableAutoUpdate }}"{{ .Values.disableAutoUpdate }}"{{ else }}"false"{{ end }}`,
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
			TemplateValue:   fmt.Sprintf("https://pl-etcd-client.%s.svc:2379", nsTmpl),
		},
	}, GlobalTemplateOptions...))
	if err != nil {
		return "", err
	}

	return secretsYAML, nil
}

func patchContainerEnvIfValueExists(containerName string, envName string, valueName string) *yamls.K8sTemplateOptions {
	placeholder := fmt.Sprintf("__%s__", strings.Replace(envName, "PL", "PX", 1))
	return &yamls.K8sTemplateOptions{
		TemplateMatcher: yamls.GenerateContainerNameMatcherFn(containerName),
		Patch:           fmt.Sprintf(`{"spec": {"template": { "spec": { "containers": [{"name": "%s", "env": [{"name": "%s"}]}] } } } }`, containerName, placeholder),
		Placeholder:     fmt.Sprintf(`- name: %s`, placeholder),
		TemplateValue: fmt.Sprintf(`{{- if .Values.%s }}
        - name: %s
          value: "{{ .Values.%s }}"
        {{- end}}`, valueName, envName, valueName),
	}
}

func generateVzDepsYAMLs(yamlMap map[string]string) (string, string, error) {
	natsYAML, err := yamls.TemplatizeK8sYAML(yamlMap[natsYAMLPath], append(GlobalTemplateOptions, []*yamls.K8sTemplateOptions{
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl:nats-server-cluster-binding"),
			Patch:           `{ "subjects": [{ "name": "nats-server", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
	}...))
	if err != nil {
		return "", "", err
	}

	etcdYAML, err := yamls.TemplatizeK8sYAML(yamlMap[etcdYAMLPath], append(GlobalTemplateOptions, []*yamls.K8sTemplateOptions{
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-etcd-pdb"),
			Patch:           `{"apiVersion" : "__PX_PDB_API_VERSION__"}`,
			Placeholder:     "__PX_PDB_API_VERSION__",
			TemplateValue:   `{{ if .Values.useBetaPdbVersion }}"policy/v1beta1"{{ else }}"policy/v1"{{ end }}`,
		},
	}...))
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

func generateVzYAMLs(yamlMap map[string]string) ([]*yamls.YAMLFile, error) {
	if _, ok := yamlMap[vizierMetadataPersistYAMLPath]; !ok {
		return nil, fmt.Errorf("Cannot generate YAMLS for specified Vizier version. Please update to latest Vizier version instead.  ")
	}

	tmplOptions := append(GlobalTemplateOptions, []*yamls.K8sTemplateOptions{
		{
			TemplateMatcher: yamls.GenerateContainerNameMatcherFn("pem"),
			Patch:           `{"spec": { "template": { "spec": { "containers": [{ "name": "pem", "resources": { "limits": { "memory": "__PX_MEMORY_LIMIT__"} } }] } } } }`,
			Placeholder:     "__PX_MEMORY_LIMIT__",
			TemplateValue:   fmt.Sprintf(`{{ if .Values.pemMemoryLimit }}"{{ .Values.pemMemoryLimit }}"{{else}}"%s"{{end}}`, defaultMemoryLimit),
		},
		{
			TemplateMatcher: yamls.GenerateContainerNameMatcherFn("pem"),
			Patch:           `{"spec": { "template": { "spec": { "containers": [{ "name": "pem", "resources": { "requests": { "memory": "__PX_MEMORY_REQUEST__"} } }] } } } }`,
			Placeholder:     "__PX_MEMORY_REQUEST__",
			TemplateValue:   fmt.Sprintf(`{{ if .Values.pemMemoryRequest }}"{{ .Values.pemMemoryRequest }}"{{else}}"%s"{{end}}`, defaultMemoryLimit),
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("proxy-envoy-config"),
			Patch:           `{}`,
			Placeholder:     ".pl.svc",
			TemplateValue:   fmt.Sprintf(".%s.svc", nsTmpl),
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cert-provisioner-binding"),
			Patch:           `{ "subjects": [{ "name": "pl-cert-provisioner-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-updater-binding"),
			Patch:           `{ "subjects": [{ "name": "pl-updater-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-updater-cluster-binding"),
			Patch:           `{ "subjects": [{ "name": "pl-updater-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-connector-cluster-binding"),
			Patch:           `{ "subjects": [{ "name": "cloud-conn-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-connector-binding"),
			Patch:           `{ "subjects": [{ "name": "cloud-conn-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-vizier-metadata-cluster-binding"),
			Patch:           `{ "subjects": [{ "name": "metadata-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-vizier-metadata-binding"),
			Patch:           `{ "subjects": [{ "name": "metadata-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-vizier-crd-metadata-binding"),
			Patch:           `{ "subjects": [{ "name": "metadata-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-vizier-crd-binding"),
			Patch:           `{ "subjects": [{ "name": "default", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-vizier-query-broker-crd-binding"),
			Patch:           `{ "subjects": [{ "name": "query-broker-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-vizier-query-broker-binding"),
			Patch:           `{ "subjects": [{ "name": "query-broker-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-vizier-metadata-node-view-cluster-binding"),
			Patch:           `{ "subjects": [{ "name": "metadata-service-account", "namespace": "__PX_SUBJECT_NAMESPACE__", "kind": "ServiceAccount" }] }`,
			Placeholder:     "__PX_SUBJECT_NAMESPACE__",
			TemplateValue:   nsTmpl,
		},
		{
			TemplateMatcher: yamls.GenerateContainerNameMatcherFn("pem"),
			Patch:           `{"spec": {"template": { "spec": { "containers": [{"name": "pem", "env": [{"name": "PL_CLOCK_CONVERTER", "value": "__PX_CLOCK_CONVERTER__"}]}] } } } }`,
			Placeholder:     "__PX_CLOCK_CONVERTER__",
			TemplateValue:   `{{ if .Values.clockConverter }}"{{ .Values.clockConverter }}"{{else}}"default"{{end}}`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("vizier-query-broker"),
			Patch:           `{"spec": {"template": {"spec": {"containers": [{"name": "app", "env": [{"name": "PL_DATA_ACCESS","value": "__PX_DATA_ACCESS__"}]}] } } } }`,
			Placeholder:     "__PX_DATA_ACCESS__",
			TemplateValue:   fmt.Sprintf(`{{ if .Values.dataAccess }}"{{ .Values.dataAccess }}"{{else}}"%s"{{end}}`, defaultDataAccess),
		},
		patchContainerEnvIfValueExists("pem", "PL_DATASTREAM_BUFFER_SIZE", "datastreamBufferSize"),
		patchContainerEnvIfValueExists("pem", "PL_DATASTREAM_BUFFER_SPIKE_SIZE", "datastreamBufferSpikeSize"),
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("vizier-metadata"),
			Patch:           `{"spec": {"template": {"spec": {"containers": [{"name": "app", "env": [{"name": "PL_RENEW_PERIOD","value": "__PX_RENEW_PERIOD__"}]}] } } } }`,
			Placeholder:     "__PX_RENEW_PERIOD__",
			TemplateValue:   fmt.Sprintf(`{{ if .Values.electionPeriodMs }}"{{ .Values.electionPeriodMs }}"{{else}}"%d"{{end}}`, defaultElectionPeriodMs),
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("vizier-cloud-connector"),
			Patch:           `{"spec": {"template": {"spec": {"containers": [{"name": "app", "env": [{"name": "PL_RENEW_PERIOD","value": "__PX_RENEW_PERIOD__"}]}] } } } }`,
			Placeholder:     "__PX_RENEW_PERIOD__",
			TemplateValue:   fmt.Sprintf(`{{ if .Values.electionPeriodMs }}"{{ .Values.electionPeriodMs }}"{{else}}"%d"{{end}}`, defaultElectionPeriodMs),
		},
		{
			TemplateMatcher: yamls.GenerateContainerNameMatcherFn("pem"),
			Patch:           `{"spec": {"template": { "spec": { "containers": [{"name": "pem", "env": [{"name": "PL_PEM_ENV_VAR_PLACEHOLDER", "value": "__PL_PEM_ENV_VAR_VALUE__"}]}] } } } }`,
			Placeholder:     `__PL_PEM_ENV_VAR_VALUE__`,
			TemplateValue: `"true" # This is un-used, and is just a placeholder used to templatize our YAMLs for Helm.
        {{- range $key, $value := .Values.customPEMFlags}}
        - name: {{$key}}
          value: "{{$value}}"
        {{- end}}`,
		},
		{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("vizier-cloud-connector"),
			Patch:           `{"spec": {"template": {"spec": {"containers": [{"name": "app", "env": [{"name": "PL_DEPLOY_KEY", "valueFrom": { "secretKeyRef": { "key": "deploy-key", "name": "__PX_DEPLOY_KEY_SECRET_NAME__", "optional": true} } }]}] }  } } }`,
			Placeholder:     "__PX_DEPLOY_KEY_SECRET_NAME__",
			TemplateValue:   `{{ if .Values.customDeployKeySecret }}"{{ .Values.customDeployKeySecret }}"{{else}}"pl-deploy-secrets"{{end}}`,
		},
	}...)

	persistentYAML, err := yamls.TemplatizeK8sYAML(yamlMap[vizierMetadataPersistYAMLPath], tmplOptions)

	if err != nil {
		return nil, err
	}
	// The persistent YAML should only be applied if --use_etcd_operator is false. The entire YAML should be wrapped in a template.
	wrappedPersistent := fmt.Sprintf(
		`{{if and (not .Values.autopilot) (not .Values.useEtcdOperator)}}
%s
{{- end}}`,
		persistentYAML)

	etcdYAML, err := yamls.TemplatizeK8sYAML(yamlMap[vizierEtcdYAMLPath], tmplOptions)
	if err != nil {
		return nil, err
	}
	// The etcd version of Vizier should only be applied if --use_etcd_operator is true. The entire YAML should be wrapped in a template.
	wrappedEtcd := fmt.Sprintf(
		`{{if and (not .Values.autopilot) .Values.useEtcdOperator}}
%s
{{- end}}`,
		etcdYAML)

	persistentAutopilotYAML, err := yamls.TemplatizeK8sYAML(yamlMap[vizierMetadataPersistAutopilotYAMLPath], tmplOptions)
	if err != nil {
		return nil, err
	}
	wrappedPersistentAP := fmt.Sprintf(
		`{{if and (.Values.autopilot) (not .Values.useEtcdOperator)}}
%s
{{- end}}`,
		persistentAutopilotYAML)

	etcdAutopilotYAML, err := yamls.TemplatizeK8sYAML(yamlMap[vizierEtcdAutopilotYAMLPath], tmplOptions)
	if err != nil {
		return nil, err
	}
	wrappedEtcdAP := fmt.Sprintf(
		`{{if and (.Values.autopilot) (.Values.useEtcdOperator)}}
%s
{{- end}}`,
		etcdAutopilotYAML)

	return []*yamls.YAMLFile{
		{
			Name: "vizier_etcd",
			YAML: wrappedEtcd,
		},
		{
			Name: "vizier_persistent",
			YAML: wrappedPersistent,
		},
		{
			Name: "vizier_etcd_ap",
			YAML: wrappedEtcdAP,
		},
		{
			Name: "vizier_persistent_ap",
			YAML: wrappedPersistentAP,
		},
	}, nil
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
kind: Secret
metadata:
  creationTimestamp: null
  name: pl-cluster-secrets
  namespace: pl
---
apiVersion: v1
kind: Secret
metadata:
  creationTimestamp: null
  name: pl-deploy-secrets
  namespace: pl
`
