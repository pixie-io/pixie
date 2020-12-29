package cmd

import (
	"errors"
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"k8s.io/client-go/kubernetes"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth"
	"k8s.io/client-go/rest"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/artifacts"
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
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
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

	yamls, err := artifacts.GenerateClusterSecretYAMLs(cloudAddr, deployKey, namespace, devCloudNamespace, kubeConfig, sentryDSN, "", false)
	if err != nil {
		return err
	}

	return k8s.ApplyYAML(clientset, kubeConfig, namespace, strings.NewReader(yamls), false)
}
