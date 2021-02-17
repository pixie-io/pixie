package artifacts

import (
	"archive/tar"
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"path"
	"strings"
	"text/template"

	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	v1 "k8s.io/api/core/v1"
	"k8s.io/client-go/rest"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	version "pixielabs.ai/pixielabs/src/shared/version/go"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

// ExtractYAMLFormat represents the types of formats we can extract YAMLs to.
type ExtractYAMLFormat int

const (
	// UnknownExtractYAMLFormat is an extraction format.
	UnknownExtractYAMLFormat ExtractYAMLFormat = iota
	// SingleFileExtractYAMLFormat extracts YAMLs to single file.
	SingleFileExtractYAMLFormat
	// MultiFileExtractYAMLFormat extract YAMLs into multiple files, according to type.
	MultiFileExtractYAMLFormat
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

// YAMLGenerator is responsible for generating and formatting Pixie YAMLs.
type YAMLGenerator struct {
	yamlMap  map[string]string // Map from YAML name -> the actual YAML.
	yamlOpts *YAMLOptions
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
	Annotations         string
	AnnotationMap       map[string]string
}

// YAMLTmplValues are the values we can substitue into our YAMLs.
type YAMLTmplValues struct {
	DeployKey string
}

// YAMLTmplArguments is a wrapper around YAMLTmplValues.
type YAMLTmplArguments struct {
	Values *YAMLTmplValues
}

// NewYAMLGenerator creates a new YAML generator.
func NewYAMLGenerator(conn *grpc.ClientConn, authToken string, versionStr string, inputVersion string, yamlOpts *YAMLOptions) (*YAMLGenerator, error) {
	yg := &YAMLGenerator{
		yamlMap:  make(map[string]string),
		yamlOpts: yamlOpts,
	}

	// Populate the YAML map.
	err := yg.generateYAMLs(conn, authToken, versionStr, inputVersion)
	if err != nil {
		return nil, err
	}

	return yg, nil
}

func (y *YAMLGenerator) generateYAMLs(conn *grpc.ClientConn, authToken string, versionStr string, inputVersion string) error {
	nsYAML, err := y.generateNamespaceYAML()
	if err != nil {
		return err
	}
	y.yamlMap["ns"] = nsYAML

	secretYAML, err := y.generateSecretYAML(versionStr, inputVersion)
	if err != nil {
		return err
	}
	y.yamlMap["secrets"] = secretYAML

	vzYAML, err := FetchVizierYAMLMap(conn, authToken, versionStr)
	if err != nil {
		return err
	}
	y.yamlMap["bootstrap"] = vzYAML[vizierBootstrapYAMLPath]

	return nil
}

func concatYAMLs(y1 string, y2 string) string {
	return y1 + "---\n" + y2
}

// ExtractYAMLs writes the generated YAMLs to a tar at the given path in the given format. If tmplValues is nil, it will not fill in the template
// and just extract the template-formatted YAML.
func (y *YAMLGenerator) ExtractYAMLs(extractPath string, format ExtractYAMLFormat, tmplValues *YAMLTmplArguments) error {
	writeYAML := func(w *tar.Writer, name string, contents string) error {
		if err := w.WriteHeader(&tar.Header{Name: name, Size: int64(len(contents)), Mode: 777}); err != nil {
			return err
		}
		if _, err := w.Write([]byte(contents)); err != nil {
			return err
		}
		return nil
	}

	filePath := path.Join(extractPath, "yamls.tar")
	writer, err := os.OpenFile(filePath, os.O_RDWR|os.O_CREATE, 0755)
	if err != nil {
		return fmt.Errorf("Failed trying to open extract_yaml path: %s", err)
	}
	defer writer.Close()
	w := tar.NewWriter(writer)

	nsYAML, err := y.GetNamespaceYAML(tmplValues)
	if err != nil {
		return err
	}

	secretsYAML, err := y.GetSecretsYAML(tmplValues)
	if err != nil {
		return err
	}

	bootstrapYAML, err := y.GetBootstrapYAML(tmplValues)
	if err != nil {
		return err
	}

	switch format {
	case MultiFileExtractYAMLFormat:
		err = writeYAML(w, fmt.Sprintf("./pixie_yamls/%02d_namespace.yaml", 0), nsYAML)
		if err != nil {
			return err
		}
		err = writeYAML(w, fmt.Sprintf("./pixie_yamls/%02d_secrets.yaml", 1), secretsYAML)
		if err != nil {
			return err
		}
		err = writeYAML(w, fmt.Sprintf("./pixie_yamls/%02d_bootstrap.yaml", 2), bootstrapYAML)
		if err != nil {
			return err
		}
		break
	case SingleFileExtractYAMLFormat:
		// Combine all YAMLs into a single file.
		combinedYAML := concatYAMLs(nsYAML, secretsYAML)
		combinedYAML = concatYAMLs(combinedYAML, bootstrapYAML)
		err = writeYAML(w, "./pixie_yamls/manifest.yaml", combinedYAML)
		if err != nil {
			return err
		}
		break
	default:
		return errors.New("Invalid extract YAML format")
	}

	if err = w.Close(); err != nil {
		if err != nil {
			return errors.New("Failed to write YAMLs")
		}
	}
	return nil
}

func (y *YAMLGenerator) generateNamespaceYAML() (string, error) {
	ns := &v1.Namespace{}
	ns.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Namespace"))
	ns.Name = y.yamlOpts.NS
	ns.Labels = y.yamlOpts.LabelMap
	ns.Annotations = y.yamlOpts.AnnotationMap

	return k8s.ConvertResourceToYAML(ns)
}

func (y *YAMLGenerator) generateSecretYAML(versionStr string, inputVersion string) (string, error) {
	dockerSecret, err := k8s.CreateDockerConfigJSONSecret(y.yamlOpts.NS, y.yamlOpts.ImagePullSecretName, y.yamlOpts.ImagePullCreds, y.yamlOpts.LabelMap, y.yamlOpts.AnnotationMap)
	if err != nil {
		return "", err
	}
	dYaml, err := k8s.ConvertResourceToYAML(dockerSecret)
	if err != nil {
		return "", err
	}

	csYAMLs, err := GenerateClusterSecretYAMLs(y.yamlOpts, "", getSentryDSN(versionStr), inputVersion)
	if err != nil {
		return "", err
	}

	return concatYAMLs(dYaml, csYAMLs), nil
}

func required(str string, value string) (string, error) {
	if value != "" {
		return value, nil
	}
	return "", errors.New("Value is required")
}

func executeTemplate(tmplValues *YAMLTmplArguments, tmplStr string) (string, error) {
	funcMap := template.FuncMap{
		"required": required,
	}

	tmpl, err := template.New("yaml").Funcs(funcMap).Parse(tmplStr)
	if err != nil {
		return "", err
	}
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, tmplValues)
	if err != nil {
		return "", err
	}

	return buf.String(), nil
}

// GetNamespaceYAML gets the namespace YAML.
func (y *YAMLGenerator) GetNamespaceYAML(tmplValues *YAMLTmplArguments) (string, error) {
	if tmplValues == nil {
		return y.yamlMap["ns"], nil
	}
	return executeTemplate(tmplValues, y.yamlMap["ns"])
}

// GetSecretsYAML gets the secrets YAML.
func (y *YAMLGenerator) GetSecretsYAML(tmplValues *YAMLTmplArguments) (string, error) {
	if tmplValues == nil {
		return y.yamlMap["secrets"], nil
	}
	return executeTemplate(tmplValues, y.yamlMap["secrets"])
}

// GetBootstrapYAML gets the bootstrap YAML.
func (y *YAMLGenerator) GetBootstrapYAML(tmplValues *YAMLTmplArguments) (string, error) {
	if tmplValues == nil {
		return y.yamlMap["bootstrap"], nil
	}
	return executeTemplate(tmplValues, y.yamlMap["bootstrap"])
}
