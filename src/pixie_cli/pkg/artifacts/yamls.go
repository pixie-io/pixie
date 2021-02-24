package artifacts

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"strings"

	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	v1 "k8s.io/api/core/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	version "pixielabs.ai/pixielabs/src/shared/version/go"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

const (
	vizierBootstrapYAMLPath = "./yamls/vizier/vizier_bootstrap_prod.yaml"
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
}

// VizierTmplValuesToMap converts the vizier template values to a map which can be used to fill out a template.
func VizierTmplValuesToMap(tmplValues *VizierTmplValues) *map[string]interface{} {
	return &map[string]interface{}{
		"DeployKey":         tmplValues.DeployKey,
		"CustomAnnotations": tmplValues.CustomAnnotations,
	}
}

// These are template options that should be applied to each resource in the Vizier YAMLs, such as annotations and labels.
var globalTemplateOptions = []*K8sTemplateOptions{
	&K8sTemplateOptions{
		Patch:       `{"metadata": { "annotations": { "__PL_ANNOTATION_KEY__": "__PL_ANNOTATION_VALUE__"} } }`,
		Placeholder: "__PL_ANNOTATION_KEY__: __PL_ANNOTATION_VALUE__",
		TemplateValue: `{{range $element := split "," .Values.CustomAnnotations -}}
    {{ $kv := split "=" $element -}}
    {{if eq (len $kv) 2 -}}
    {{ index $kv 0}}: "{{ index $kv 1}}"
    {{- end}}
    {{end}}`,
	},
	&K8sTemplateOptions{
		TemplateMatcher: TemplateScopeMatcher,
		Patch:           `{"spec": { "template": { "metadata": { "annotations": { "__PL_SPEC_ANNOTATION_KEY__": "__PL_SPEC_ANNOTATION_VALUE__"} } } } }`,
		Placeholder:     "__PL_SPEC_ANNOTATION_KEY__: __PL_SPEC_ANNOTATION_VALUE__",
		TemplateValue: `{{range $element := split "," .Values.CustomAnnotations -}}
        {{ $kv := split "=" $element -}}
        {{if eq (len $kv) 2 -}}
        {{ index $kv 0}}: "{{ index $kv 1}}"
        {{- end}}
        {{end}}`,
	},
}

func downloadFile(url string) (io.ReadCloser, error) {
	// Get the data
	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}
	return resp.Body, nil
}

func downloadVizierYAMLs(conn *grpc.ClientConn, authToken, versionStr string) (io.ReadCloser, error) {
	client := cloudapipb.NewArtifactTrackerClient(conn)

	req := &cloudapipb.GetDownloadLinkRequest{
		ArtifactName: "vizier",
		VersionStr:   versionStr,
		ArtifactType: cloudapipb.AT_CONTAINER_SET_YAMLS,
	}
	ctxWithCreds := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", authToken))

	resp, err := client.GetDownloadLink(ctxWithCreds, req)
	if err != nil {
		return nil, err
	}

	return downloadFile(resp.Url)
}

// FetchVizierYAMLMap fetches Vizier YAML files and write to a map <fname>:<yaml string>.
func FetchVizierYAMLMap(conn *grpc.ClientConn, authToken, versionStr string) (map[string]string, error) {
	reader, err := downloadVizierYAMLs(conn, authToken, versionStr)

	if err != nil {
		return nil, err
	}
	defer reader.Close()

	yamlMap, err := utils.ReadTarFileFromReader(reader)
	if err != nil {
		return nil, err
	}
	return yamlMap, nil
}

func getSentryDSN(vizierVersion string) string {
	// Only we have dev CLI.
	if version.GetVersion().IsDev() {
		return devSentryDSN
	}
	// If it contains - it must be a pre-release Vizier.
	if strings.Contains(vizierVersion, "-") {
		return devSentryDSN
	}
	return prodSentryDSN
}

// YAMLOptions are used for generating Pixie YAMLs. As we introduce templates, more of these will be moved to the template scheme.
type YAMLOptions struct {
	NS                  string
	CloudAddr           string
	ImagePullSecretName string
	ImagePullCreds      string
	DevCloudNS          string
	KubeConfig          *rest.Config
	UseEtcdOperator     bool
	Labels              string
	LabelMap            map[string]string
}

// GenerateTemplatedDeployYAMLs generates the YAMLs that should be run when deploying Pixie.
func GenerateTemplatedDeployYAMLs(clientset *kubernetes.Clientset, conn *grpc.ClientConn, authToken string, versionStr string, inputVersion string, yamlOpts *YAMLOptions) ([]*YAMLFile, error) {
	nsYAML, err := generateNamespaceYAML(clientset, yamlOpts)
	if err != nil {
		return nil, err
	}

	secretsYAML, err := generateSecretsYAML(clientset, yamlOpts, versionStr, inputVersion)
	if err != nil {
		return nil, err
	}

	vzYAML, err := generateBootstrapYAML(clientset, conn, authToken, versionStr)
	if err != nil {
		return nil, err
	}

	return []*YAMLFile{
		&YAMLFile{
			Name: "namespace",
			YAML: nsYAML,
		},
		&YAMLFile{
			Name: "secrets",
			YAML: secretsYAML,
		},
		&YAMLFile{
			Name: "bootstrap",
			YAML: vzYAML,
		},
	}, nil
}

// generateNamespaceYAML creates the YAML for the namespace Pixie is deployed in.
func generateNamespaceYAML(clientset *kubernetes.Clientset, yamlOpts *YAMLOptions) (string, error) {
	ns := &v1.Namespace{}
	ns.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Namespace"))
	ns.Name = yamlOpts.NS
	ns.Labels = yamlOpts.LabelMap

	origYAML, err := k8s.ConvertResourceToYAML(ns)
	if err != nil {
		return "", err
	}

	nsYAML, err := TemplatizeK8sYAML(clientset, origYAML, globalTemplateOptions)
	if err != nil {
		return "", err
	}

	return nsYAML, nil
}

// generateSecretsYAML creates the YAML for Pixie secrets.
func generateSecretsYAML(clientset *kubernetes.Clientset, yamlOpts *YAMLOptions, versionStr string, inputVersion string) (string, error) {
	dockerSecret, err := k8s.CreateDockerConfigJSONSecret(yamlOpts.NS, yamlOpts.ImagePullSecretName, yamlOpts.ImagePullCreds, yamlOpts.LabelMap, nil)
	if err != nil {
		return "", err
	}
	dYaml, err := k8s.ConvertResourceToYAML(dockerSecret)
	if err != nil {
		return "", err
	}

	csYAMLs, err := GenerateClusterSecretYAMLs(yamlOpts, "", getSentryDSN(versionStr), inputVersion)
	if err != nil {
		return "", err
	}

	origYAML := concatYAMLs(dYaml, csYAMLs)

	// Fill in configmaps.
	secretsYAML, err := TemplatizeK8sYAML(clientset, origYAML, append([]*K8sTemplateOptions{
		&K8sTemplateOptions{
			TemplateMatcher: GenerateResourceNameMatcherFn("pl-deploy-secrets"),
			Patch:           `{"stringData": { "deploy-key": "__PL_DEPLOY_KEY__"} }`,
			Placeholder:     "__PL_DEPLOY_KEY__",
			TemplateValue:   `{{ .Values.DeployKey | required "...." }}`,
		},
		&K8sTemplateOptions{
			TemplateMatcher: GenerateResourceNameMatcherFn("pl-cluster-config"),
			Patch:           `{"data": { "PL_CUSTOM_ANNOTATIONS": "__PL_CUSTOM_ANNOTATIONS__"} }`,
			Placeholder:     "__PL_CUSTOM_ANNOTATIONS__",
			TemplateValue:   `"{{ .Values.CustomAnnotations }}"`,
		},
	}, globalTemplateOptions...))
	if err != nil {
		return "", err
	}

	return secretsYAML, nil
}

// generateBootstrapYAML creates the YAML for the Pixie bootstrap resources.
func generateBootstrapYAML(clientset *kubernetes.Clientset, conn *grpc.ClientConn, authToken string, versionStr string) (string, error) {
	vzMap, err := FetchVizierYAMLMap(conn, authToken, versionStr)
	if err != nil {
		return "", nil
	}

	vzBootstrapYAML, err := TemplatizeK8sYAML(clientset, vzMap[vizierBootstrapYAMLPath], globalTemplateOptions)
	if err != nil {
		return "", err
	}

	return vzBootstrapYAML, nil
}
