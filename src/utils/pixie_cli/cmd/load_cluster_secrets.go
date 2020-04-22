package cmd

import (
	"crypto/rand"
	"errors"
	"fmt"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"k8s.io/client-go/kubernetes"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/k8s"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
)

// LoadClusterSecretsCmd loads cluster secretss
var LoadClusterSecretsCmd = &cobra.Command{
	Use:   "load-cluster-secrets",
	Short: "Loads the cluster secrets on the current K8s cluster",
	Run: func(cmd *cobra.Command, args []string) {
		clusterID, _ := cmd.Flags().GetString("cluster_id")
		namespace, _ := cmd.Flags().GetString("namespace")
		devCloudNamespace, _ := cmd.Flags().GetString("dev_cloud_namespace")

		cloudAddr := viper.GetString("cloud_addr")
		kubeConfig := k8s.GetConfig()
		clientset := k8s.GetClientset(kubeConfig)

		err := LoadClusterSecrets(clientset, cloudAddr, clusterID, namespace, devCloudNamespace, kubeConfig, "")
		if err != nil {
			log.WithError(err).Fatal("Could not load cluster secrets")
		}
	},
}

func init() {
	LoadClusterSecretsCmd.Flags().StringP("cluster_id", "i", "", "The ID of the cluster")
	viper.BindPFlag("cluster_id", LoadClusterSecretsCmd.Flags().Lookup("cluster_id"))

	LoadClusterSecretsCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", LoadClusterSecretsCmd.Flags().Lookup("namespace"))

	LoadClusterSecretsCmd.Flags().StringP("dev_cloud_namespace", "m", "", "The namespace of Pixie Cloud, if running Cloud on minikube")
	viper.BindPFlag("dev_cloud_namespace", LoadClusterSecretsCmd.Flags().Lookup("dev_cloud_namespace"))
}

// LoadClusterSecrets loads Vizier's secrets and configmap to the given namespace.
func LoadClusterSecrets(clientset *kubernetes.Clientset, cloudAddr string, clusterID string, namespace string, devCloudNamespace string, kubeConfig *rest.Config, sentryDSN string) error {
	if clusterID == "" {
		return errors.New("cluster_id is required")
	}

	err := createCloudConfig(clientset, cloudAddr, namespace, devCloudNamespace, kubeConfig)
	if err != nil {
		return err
	}

	err = createClusterSecrets(clientset, clusterID, sentryDSN, namespace)
	if err != nil {
		return err
	}

	return nil

}

func getK8sVersion(kubeConfig *rest.Config) (string, error) {
	discoveryClient := k8s.GetDiscoveryClient(kubeConfig)
	version, err := discoveryClient.ServerVersion()
	if err != nil {
		return "", err
	}
	return version.GitVersion, nil
}

func createCloudConfig(clientset *kubernetes.Clientset, cloudAddr string, namespace string, devCloudNamespace string, kubeConfig *rest.Config) error {
	// Attempt to delete an existing pl-cloud-config configmap.
	_ = k8s.DeleteConfigMap(clientset, "pl-cloud-config", "pl")
	// devCloudNamespace implies we are running in a dev enivironment and we should attach to
	// vzconn in that namespace.
	if devCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNamespace)
	}

	clusterName := getCurrentCluster()

	clusterVersion, err := getK8sVersion(kubeConfig)
	if err != nil {
		return errors.New("Could not get cluster version")
	}

	err = k8s.CreateConfigMapFromLiterals(clientset, namespace, "pl-cloud-config", map[string]string{
		"PL_CLOUD_ADDR":      cloudAddr,
		"PL_CLUSTER_NAME":    clusterName,
		"PL_CLUSTER_VERSION": clusterVersion,
	})

	return err
}

func createClusterSecrets(clientset *kubernetes.Clientset, clusterID, sentryDSN, namespace string) error {
	jwtSigningKey := make([]byte, 64)
	_, err := rand.Read(jwtSigningKey)
	if err != nil {
		return errors.New("Could not generate JWT signing key")
	}

	// Load clusterID and JWT signing key as a secret.
	k8s.DeleteSecret(clientset, namespace, "pl-cluster-secrets")
	err = k8s.CreateGenericSecretFromLiterals(clientset, namespace, "pl-cluster-secrets", map[string]string{
		"cluster-id":      clusterID,
		"sentry-dsn":      sentryDSN,
		"jwt-signing-key": fmt.Sprintf("%x", jwtSigningKey),
	})
	if err != nil {
		return err
	}

	return nil
}
