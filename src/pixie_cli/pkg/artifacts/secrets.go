package artifacts

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"strings"

	"k8s.io/client-go/rest"

	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

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
  PL_BOOTSTRAP_MODE: "true"
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

func getK8sVersion(kubeConfig *rest.Config) (string, error) {
	discoveryClient := k8s.GetDiscoveryClient(kubeConfig)
	version, err := discoveryClient.ServerVersion()
	if err != nil {
		return "", err
	}
	return version.GitVersion, nil
}

func getCurrentCluster() string {
	kcmd := exec.Command("kubectl", "config", "current-context")
	var out bytes.Buffer
	kcmd.Stdout = &out
	kcmd.Stderr = os.Stderr
	err := kcmd.Run()

	if err != nil {
		return ""
	}
	return out.String()
}

// SetConfigValues sets the values for the template, based on the user-provided flags.
func SetConfigValues(kubeconfig *rest.Config, tmplValues *VizierTmplValues, cloudAddr, devCloudNS, clusterName string) {
	yamlCloudAddr := cloudAddr
	updateCloudAddr := cloudAddr
	// devCloudNamespace implies we are running in a dev enivironment and we should attach to
	// vzconn in that namespace.
	if devCloudNS != "" {
		yamlCloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNS)
		updateCloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", devCloudNS)
	}

	tmplValues.CloudAddr = yamlCloudAddr
	tmplValues.CloudUpdateAddr = updateCloudAddr

	if clusterName == "" && kubeconfig != nil { // Only record cluster name if we are deploying directly to the current cluster.
		clusterName = getCurrentCluster()
	}
	tmplValues.ClusterName = clusterName
}

// GenerateClusterSecretYAMLs generates YAMLs for the cluster secrets.
func GenerateClusterSecretYAMLs(sentryDSN string) (string, error) {
	// Perform substitutions.
	r := strings.NewReplacer(
		"__PL_SENTRY_DSN__", sentryDSN,
	)
	return r.Replace(secretsYAML), nil
}
