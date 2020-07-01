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

// GenerateClusterSecretYAMLs generates YAMLs for the cluster secrets.
func GenerateClusterSecretYAMLs(cloudAddr string, deployKey string, namespace string, devCloudNamespace string, kubeConfig *rest.Config, sentryDSN string, version string, useEtcdOperator bool) (string, error) {
	yamls := make([]string, 5)
	ccYaml, err := createCloudConfigYAML(cloudAddr, namespace, devCloudNamespace, kubeConfig)
	if err != nil {
		return "", err
	}
	yamls[0] = ccYaml

	clusterYaml, err := createClusterConfigYAML(namespace, useEtcdOperator)
	if err != nil {
		return "", err
	}
	yamls[1] = clusterYaml

	csYaml, err := createClusterSecretsYAML(sentryDSN, namespace)
	if err != nil {
		return "", err
	}
	yamls[2] = csYaml

	bYaml, err := createBootstrapConfigMapYAML(namespace, version)
	if err != nil {
		return "", err
	}
	yamls[3] = bYaml

	dYaml, err := createDeploySecretsYAML(namespace, deployKey)
	if err != nil {
		return "", err
	}
	yamls[4] = dYaml

	return "---\n" + strings.Join(yamls, "\n---\n"), nil
}

func getK8sVersion(kubeConfig *rest.Config) (string, error) {
	discoveryClient := k8s.GetDiscoveryClient(kubeConfig)
	version, err := discoveryClient.ServerVersion()
	if err != nil {
		return "", err
	}
	return version.GitVersion, nil
}

func createDeploySecretsYAML(namespace string, deployKey string) (string, error) {
	cm, err := k8s.CreateGenericSecretFromLiterals(namespace, "pl-deploy-secrets", map[string]string{
		"deploy-key": deployKey,
	})
	if err != nil {
		return "", err
	}
	return k8s.ConvertResourceToYAML(cm)
}

func createClusterConfigYAML(namespace string, useEtcdOperator bool) (string, error) {
	etcdAddr := "https://etcd.pl.svc:2379"
	if useEtcdOperator {
		etcdAddr = "https://pl-etcd-client.pl.svc:2379"
	}

	cm, err := k8s.CreateConfigMapFromLiterals(namespace, "pl-cluster-config", map[string]string{
		"PL_ETCD_OPERATOR_ENABLED": strconv.FormatBool(useEtcdOperator),
		"PL_MD_ETCD_SERVER":        etcdAddr,
	})
	if err != nil {
		return "", err
	}
	return k8s.ConvertResourceToYAML(cm)
}

func createCloudConfigYAML(cloudAddr string, namespace string, devCloudNamespace string, kubeConfig *rest.Config) (string, error) {
	// devCloudNamespace implies we are running in a dev enivironment and we should attach to
	// vzconn in that namespace.
	updateCloudAddr := cloudAddr
	if devCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNamespace)
		updateCloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", devCloudNamespace)
	}

	clusterName := ""

	if kubeConfig != nil { // Only record cluster name if we are deploying directly to the current cluster.
		clusterName = getCurrentCluster()
	}

	cm, err := k8s.CreateConfigMapFromLiterals(namespace, "pl-cloud-config", map[string]string{
		"PL_CLOUD_ADDR":        cloudAddr,
		"PL_CLUSTER_NAME":      clusterName,
		"PL_UPDATE_CLOUD_ADDR": updateCloudAddr,
	})
	if err != nil {
		return "", err
	}
	return k8s.ConvertResourceToYAML(cm)
}

func createClusterSecretsYAML(sentryDSN, namespace string) (string, error) {
	s, err := k8s.CreateGenericSecretFromLiterals(namespace, "pl-cluster-secrets", map[string]string{
		"sentry-dsn": sentryDSN,
	})
	if err != nil {
		return "", err
	}
	return k8s.ConvertResourceToYAML(s)
}

func createBootstrapConfigMapYAML(namespace, version string) (string, error) {
	cm, err := k8s.CreateConfigMapFromLiterals(namespace, "pl-cloud-connector-bootstrap-config", map[string]string{
		"PL_BOOTSTRAP_MODE":    "true",
		"PL_BOOTSTRAP_VERSION": version,
	})
	if err != nil {
		return "", err
	}
	cm.Labels = make(map[string]string)
	cm.Labels["component"] = "vizier"

	return k8s.ConvertResourceToYAML(cm)
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
