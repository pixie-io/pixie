package artifacts

import (
	"bytes"
	"fmt"
	"k8s.io/client-go/rest"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

const secretsYAML string = `
apiVersion: v1
data:
  PL_CLOUD_ADDR: "__PL_CLOUD_ADDR__"
  PL_CLUSTER_NAME: "__PL_CLUSTER_NAME__"
  PL_UPDATE_CLOUD_ADDR: __PL_UPDATE_CLOUD_ADDR__
kind: ConfigMap
metadata:
  creationTimestamp: null
  name: pl-cloud-config
  namespace: pl
  labels:
    __PL_CUSTOM_LABEL_STRING__
---
apiVersion: v1
data:
  PL_ETCD_OPERATOR_ENABLED: "__PL_ETCD_OPERATOR_ENABLED__"
  PL_MD_ETCD_SERVER: __PL_MD_ETCD_SERVER__
  PL_CUSTOM_LABELS: "__PL_CUSTOM_LABELS__"
kind: ConfigMap
metadata:
  creationTimestamp: null
  name: pl-cluster-config
  namespace: pl
  labels:
    __PL_CUSTOM_LABEL_STRING__
---
apiVersion: v1
stringData:
  sentry-dsn: __PL_SENTRY_DSN__
kind: Secret
metadata:
  creationTimestamp: null
  name: pl-cluster-secrets
  namespace: pl
  labels:
    __PL_CUSTOM_LABEL_STRING__
---
apiVersion: v1
data:
  PL_BOOTSTRAP_MODE: "true"
  PL_BOOTSTRAP_VERSION: "__PL_BOOTSTRAP_VERSION__"
kind: ConfigMap
metadata:
  creationTimestamp: null
  labels:
    component: vizier
    __PL_CUSTOM_LABEL_STRING__
  name: pl-cloud-connector-bootstrap-config
  namespace: pl
---
apiVersion: v1
kind: Secret
metadata:
  creationTimestamp: null
  name: pl-deploy-secrets
  namespace: pl
  labels:
    __PL_CUSTOM_LABEL_STRING__
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

// GenerateClusterSecretYAMLs generates YAMLs for the cluster secrets.
func GenerateClusterSecretYAMLs(yamlOpts *YAMLOptions, deployKey string, sentryDSN string, version string) (string, error) {
	// devCloudNamespace implies we are running in a dev enivironment and we should attach to
	// vzconn in that namespace.
	cloudAddr := yamlOpts.CloudAddr
	updateCloudAddr := yamlOpts.CloudAddr
	devCloudNamespace := yamlOpts.DevCloudNS

	if devCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNamespace)
		updateCloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", devCloudNamespace)
	}

	clusterName := ""

	if yamlOpts.KubeConfig != nil { // Only record cluster name if we are deploying directly to the current cluster.
		clusterName = getCurrentCluster()
	}

	etcdAddr := "https://etcd.pl.svc:2379"
	if yamlOpts.UseEtcdOperator {
		etcdAddr = "https://pl-etcd-client.pl.svc:2379"
	}

	labelString := ""
	lm := yamlOpts.LabelMap
	if lm != nil {
		for k, v := range lm {
			labelString += fmt.Sprintf("%s: \"%s\"\n    ", k, v)
		}
	}

	// Perform substitutions.
	r := strings.NewReplacer(
		"__PL_CLOUD_ADDR__", cloudAddr,
		"__PL_CLUSTER_NAME__", clusterName,
		"__PL_UPDATE_CLOUD_ADDR__", updateCloudAddr,
		"__PL_ETCD_OPERATOR_ENABLED__", strconv.FormatBool(yamlOpts.UseEtcdOperator),
		"__PL_MD_ETCD_SERVER__", etcdAddr,
		"__PL_SENTRY_DSN__", sentryDSN,
		"__PL_BOOTSTRAP_VERSION__", version,
		"__PL_CUSTOM_LABELS__", yamlOpts.Labels,
		"__PL_CUSTOM_LABEL_STRING__", labelString,
	)
	return r.Replace(secretsYAML), nil
}
