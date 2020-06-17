package cmd

import (
	"errors"
	"fmt"
	"strconv"
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"k8s.io/client-go/kubernetes"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
)

// LoadClusterSecretsCmd loads cluster secretss
var LoadClusterSecretsCmd = &cobra.Command{
	Use:   "load-cluster-secrets",
	Short: "Loads the cluster secrets on the current K8s cluster",
	Run: func(cmd *cobra.Command, args []string) {
		deployKey, _ := cmd.Flags().GetString("deploy_key")
		namespace, _ := cmd.Flags().GetString("namespace")
		devCloudNamespace, _ := cmd.Flags().GetString("dev_cloud_namespace")

		cloudAddr := viper.GetString("cloud_addr")
		kubeConfig := k8s.GetConfig()
		clientset := k8s.GetClientset(kubeConfig)

		err := LoadClusterSecrets(clientset, cloudAddr, deployKey, namespace, devCloudNamespace, kubeConfig, "")
		if err != nil {
			log.WithError(err).Fatal("Could not load cluster secrets")
		}
	},
}

func init() {
	LoadClusterSecretsCmd.Flags().StringP("deploy_key", "k", "", "The deploy key to use to deploy the cluster")
	viper.BindPFlag("deploy_key", LoadClusterSecretsCmd.Flags().Lookup("deploy_key"))

	LoadClusterSecretsCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", LoadClusterSecretsCmd.Flags().Lookup("namespace"))

	LoadClusterSecretsCmd.Flags().StringP("dev_cloud_namespace", "m", "", "The namespace of Pixie Cloud, if running Cloud on minikube")
	viper.BindPFlag("dev_cloud_namespace", LoadClusterSecretsCmd.Flags().Lookup("dev_cloud_namespace"))
}

// LoadClusterSecrets loads Vizier's secrets and configmap to the given namespace.
func LoadClusterSecrets(clientset *kubernetes.Clientset, cloudAddr string, deployKey string, namespace string, devCloudNamespace string, kubeConfig *rest.Config, sentryDSN string) error {
	if deployKey == "" {
		return errors.New("deploy_key is required")
	}

	// Attempt to delete an existing pl-cloud-config configmap.
	_ = k8s.DeleteConfigMap(clientset, "pl-cloud-config", "pl")
	k8s.DeleteSecret(clientset, namespace, "pl-cluster-secrets")

	yamls, err := GenerateClusterSecretYAMLs(cloudAddr, deployKey, namespace, devCloudNamespace, kubeConfig, sentryDSN, "", false)
	if err != nil {
		return err
	}

	return k8s.ApplyYAML(clientset, kubeConfig, namespace, strings.NewReader(yamls))
}

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
	if devCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNamespace)
	}

	clusterName := ""

	if kubeConfig != nil { // Only record cluster name if we are deploying directly to the current cluster.
		clusterName = getCurrentCluster()
	}

	cm, err := k8s.CreateConfigMapFromLiterals(namespace, "pl-cloud-config", map[string]string{
		"PL_CLOUD_ADDR":   cloudAddr,
		"PL_CLUSTER_NAME": clusterName,
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
