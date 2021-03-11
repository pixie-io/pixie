package vizieryamls

import (
	"fmt"
	"os"
	"strconv"
	"strings"

	v1 "k8s.io/api/core/v1"
	"k8s.io/client-go/kubernetes"

	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
	"pixielabs.ai/pixielabs/src/utils/shared/tar"
	"pixielabs.ai/pixielabs/src/utils/shared/yamls"
)

const (
	vizierBootstrapYAMLPath       = "./yamls/vizier/vizier_bootstrap_prod.yaml"
	etcdOperatorYAMLPath          = "./yamls/vizier_deps/etcd_operator_prod.yaml"
	vizierEtcdYAMLPath            = "./yamls/vizier/vizier_etcd_metadata_prod.yaml"
	vizierMetadataPersistYAMLPath = "./yamls/vizier/vizier_metadata_persist_prod.yaml"
	natsYAMLPath                  = "./yamls/vizier_deps/nats_prod.yaml"
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
}

// VizierTmplValuesToMap converts the vizier template values to a map which can be used to fill out a template.
func VizierTmplValuesToMap(tmplValues *VizierTmplValues) *map[string]interface{} {
	return &map[string]interface{}{
		"deployKey":         tmplValues.DeployKey,
		"customAnnotations": tmplValues.CustomAnnotations,
		"customLabels":      tmplValues.CustomLabels,
		"cloudAddr":         tmplValues.CloudAddr,
		"clusterName":       tmplValues.ClusterName,
		"cloudUpdateAddr":   tmplValues.CloudUpdateAddr,
		"useEtcdOperator":   tmplValues.UseEtcdOperator,
		"bootstrapVersion":  tmplValues.BootstrapVersion,
	}
}

// These are template options that should be applied to each resource in the Vizier YAMLs, such as annotations and labels.
var globalTemplateOptions = []*yamls.K8sTemplateOptions{
	&yamls.K8sTemplateOptions{
		Patch:       `{"metadata": { "annotations": { "__PL_ANNOTATION_KEY__": "__PL_ANNOTATION_VALUE__"} } }`,
		Placeholder: "__PL_ANNOTATION_KEY__: __PL_ANNOTATION_VALUE__",
		TemplateValue: `{{if .Values.customAnnotations}}{{range $element := split "," .Values.customAnnotations -}}
    {{ $kv := split "=" $element -}}
    {{if eq (len $kv) 2 -}}
    {{ $kv._0 }}: "{{ $kv._1 }}"
    {{- end}}
    {{end}}{{end}}`,
	},
	&yamls.K8sTemplateOptions{
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
	&yamls.K8sTemplateOptions{
		Patch:       `{"metadata": { "labels": { "__PL_LABEL_KEY__": "__PL_LABEL_VALUE__"} } }`,
		Placeholder: "__PL_LABEL_KEY__: __PL_LABEL_VALUE__",
		TemplateValue: `{{if .Values.customLabels}}{{range $element := split "," .Values.customLabels -}}
    {{ $kv := split "=" $element -}}
    {{if eq (len $kv) 2 -}}
    {{ $kv._0 }}: "{{ $kv._1 }}"
    {{- end}}
    {{end}}{{end}}`,
	},
	&yamls.K8sTemplateOptions{
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
}

func getSentryDSN(vizierVersion string) string {
	// If it contains - it must be a pre-release Vizier.
	if strings.Contains(vizierVersion, "-") {
		return devSentryDSN
	}
	return prodSentryDSN
}

// GenerateTemplatedDeployYAMLsWithTar generates the YAMLs that should be run when deploying Pixie using the provided tar file.
func GenerateTemplatedDeployYAMLsWithTar(clientset *kubernetes.Clientset, tarPath string, versionStr string, ns string) ([]*yamls.YAMLFile, error) {
	file, err := os.Open(tarPath)
	if err != nil {
		return nil, err
	}

	yamlMap, err := tar.ReadTarFileFromReader(file)
	if err != nil {
		return nil, err
	}

	return generateTemplatedDeployYAMLs(clientset, yamlMap, versionStr, ns, "", "")
}

// GenerateTemplatedDeployBootstrapYAMLs generates the YAMLs that should be run when deploying Pixie with Bootstrap mode.
func GenerateTemplatedDeployBootstrapYAMLs(clientset *kubernetes.Clientset, yamlMap map[string]string, versionStr string, ns string, imagePullSecretName string, imagePullCreds string) ([]*yamls.YAMLFile, error) {
	nsYAML, err := generateNamespaceYAML(clientset, ns)
	if err != nil {
		return nil, err
	}

	secretsYAML, err := GenerateSecretsYAML(clientset, ns, imagePullSecretName, imagePullCreds, versionStr, true)
	if err != nil {
		return nil, err
	}

	vzYAML, err := generateBootstrapYAML(clientset, yamlMap)
	if err != nil {
		return nil, err
	}

	return []*yamls.YAMLFile{
		&yamls.YAMLFile{
			Name: "namespace",
			YAML: nsYAML,
		},
		&yamls.YAMLFile{
			Name: "secrets",
			YAML: secretsYAML,
		},
		&yamls.YAMLFile{
			Name: "bootstrap",
			YAML: vzYAML,
		},
	}, nil
}

func generateTemplatedDeployYAMLs(clientset *kubernetes.Clientset, yamlMap map[string]string, versionStr string, ns string, imagePullSecretName string, imagePullCreds string) ([]*yamls.YAMLFile, error) {
	nsYAML, err := generateNamespaceYAML(clientset, ns)
	if err != nil {
		return nil, err
	}

	secretsYAML, err := GenerateSecretsYAML(clientset, ns, imagePullSecretName, imagePullCreds, versionStr, false)
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
		&yamls.YAMLFile{
			Name: "namespace",
			YAML: nsYAML,
		},
		&yamls.YAMLFile{
			Name: "secrets",
			YAML: secretsYAML,
		},
		&yamls.YAMLFile{
			Name: "nats",
			YAML: natsYAML,
		},
		&yamls.YAMLFile{
			Name: "etcd",
			YAML: etcdYAML,
		},
		&yamls.YAMLFile{
			Name: "vizier_etcd",
			YAML: etcdVzYAML,
		},
		&yamls.YAMLFile{
			Name: "vizier_persistent",
			YAML: persistentVzYAML,
		},
	}, nil
}

// generateNamespaceYAML creates the YAML for the namespace Pixie is deployed in.
func generateNamespaceYAML(clientset *kubernetes.Clientset, namespace string) (string, error) {
	ns := &v1.Namespace{}
	ns.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Namespace"))
	ns.Name = namespace

	origYAML, err := k8s.ConvertResourceToYAML(ns)
	if err != nil {
		return "", err
	}

	nsYAML, err := yamls.TemplatizeK8sYAML(clientset, origYAML, globalTemplateOptions)
	if err != nil {
		return "", err
	}

	return nsYAML, nil
}

// GenerateSecretsYAML creates the YAML for Pixie secrets.
func GenerateSecretsYAML(clientset *kubernetes.Clientset, ns string, imagePullSecretName string, imagePullCreds string, versionStr string, bootstrapMode bool) (string, error) {
	dockerYAML := ""

	// Only add docker image secrets if specified.
	if imagePullCreds != "" {
		dockerSecret, err := k8s.CreateDockerConfigJSONSecret(ns, imagePullSecretName, imagePullCreds)
		if err != nil {
			return "", err
		}
		dYaml, err := k8s.ConvertResourceToYAML(dockerSecret)
		if err != nil {
			return "", err
		}
		dockerYAML = dYaml
	}

	csYAMLs, err := generateClusterSecretYAMLs(getSentryDSN(versionStr), bootstrapMode)
	if err != nil {
		return "", err
	}

	origYAML := yamls.ConcatYAMLs(dockerYAML, csYAMLs)

	// Fill in configmaps.
	secretsYAML, err := yamls.TemplatizeK8sYAML(clientset, origYAML, append([]*yamls.K8sTemplateOptions{
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-deploy-secrets"),
			Patch:           `{"stringData": { "deploy-key": "__PL_DEPLOY_KEY__"} }`,
			Placeholder:     "__PL_DEPLOY_KEY__",
			TemplateValue:   `"{{ .Values.deployKey }}"`,
		},
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_CUSTOM_ANNOTATIONS": "__PL_CUSTOM_ANNOTATIONS__"} }`,
			Placeholder:     "__PL_CUSTOM_ANNOTATIONS__",
			TemplateValue:   `"{{ .Values.customAnnotations }}"`,
		},
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_CUSTOM_LABELS": "__PL_CUSTOM_LABELS__"} }`,
			Placeholder:     "__PL_CUSTOM_LABELS__",
			TemplateValue:   `"{{ .Values.customLabels }}"`,
		},
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-config"),
			Patch:           `{"data": { "PL_CLOUD_ADDR": "__PL_CLOUD_ADDR__"} }`,
			Placeholder:     "__PL_CLOUD_ADDR__",
			TemplateValue:   `{{ if .Values.cloudAddr }}"{{ .Values.cloudAddr }}"{{ else }}"withpixie.ai:443"{{ end }}`,
		},
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-config"),
			Patch:           `{"data": { "PL_UPDATE_CLOUD_ADDR": "__PL_UPDATE_CLOUD_ADDR__"} }`,
			Placeholder:     "__PL_UPDATE_CLOUD_ADDR__",
			TemplateValue:   `{{ if .Values.cloudUpdateAddr }}"{{ .Values.cloudUpdateAddr }}"{{ else }}"withpixie.ai:443"{{ end }}`,
		},
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-config"),
			Patch:           `{"data": { "PL_CLUSTER_NAME": "__PL_CLUSTER_NAME__"} }`,
			Placeholder:     "__PL_CLUSTER_NAME__",
			TemplateValue:   `"{{ .Values.clusterName }}"`,
		},
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_ETCD_OPERATOR_ENABLED": "__PL_ETCD_OPERATOR_ENABLED__"} }`,
			Placeholder:     "__PL_ETCD_OPERATOR_ENABLED__",
			TemplateValue:   `{{ if .Values.useEtcdOperator }}"true"{{else}}"false"{{end}}`,
		},
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_MD_ETCD_SERVER": "__PL_MD_ETCD_SERVER__"} }`,
			Placeholder:     "__PL_MD_ETCD_SERVER__",
			TemplateValue:   `{{ if .Values.useEtcdOperator }}"https://pl-etcd-client.pl.svc:2379"{{else}}"https://etcd.pl.svc:2379"{{end}}`,
		},
		&yamls.K8sTemplateOptions{
			TemplateMatcher: yamls.GenerateResourceNameMatcherFn("pl-cloud-connector-bootstrap-config"),
			Patch:           `{"data": { "PL_BOOTSTRAP_VERSION": "__PL_BOOTSTRAP_VERSION__"} }`,
			Placeholder:     "__PL_BOOTSTRAP_VERSION__",
			TemplateValue:   `"{{.Values.bootstrapVersion}}"`,
		},
	}, globalTemplateOptions...))
	if err != nil {
		return "", err
	}

	return secretsYAML, nil
}

// generateBootstrapYAML creates the YAML for the Pixie bootstrap resources.
func generateBootstrapYAML(clientset *kubernetes.Clientset, yamlMap map[string]string) (string, error) {
	vzBootstrapYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[vizierBootstrapYAMLPath], globalTemplateOptions)
	if err != nil {
		return "", err
	}

	return vzBootstrapYAML, nil
}

func generateVzDepsYAMLs(clientset *kubernetes.Clientset, yamlMap map[string]string) (string, string, error) {
	natsYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[natsYAMLPath], globalTemplateOptions)
	if err != nil {
		return "", "", err
	}

	etcdYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[etcdOperatorYAMLPath], globalTemplateOptions)
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

	persistentYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[vizierMetadataPersistYAMLPath], globalTemplateOptions)
	if err != nil {
		return "", "", err
	}
	// The persistent YAML should only be applied if --use_etcd_operator is false. The entire YAML should be wrapped in a template.
	wrappedPersistent := fmt.Sprintf(
		`{{if not .Values.useEtcdOperator}}
%s
{{- end}}`,
		persistentYAML)

	etcdYAML, err := yamls.TemplatizeK8sYAML(clientset, yamlMap[vizierEtcdYAMLPath], globalTemplateOptions)
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
