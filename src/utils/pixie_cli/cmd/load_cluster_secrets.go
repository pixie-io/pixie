package cmd

import (
	"crypto/rand"
	"errors"
	"fmt"
	"os/exec"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"k8s.io/client-go/kubernetes"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/k8s"
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

		err := LoadClusterSecrets(clientset, cloudAddr, clusterID, namespace, devCloudNamespace)
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
func LoadClusterSecrets(clientset *kubernetes.Clientset, cloudAddr string, clusterID string, namespace string, devCloudNamespace string) error {
	if clusterID == "" {
		return errors.New("cluster_id is required")
	}

	err := createCloudConfig(cloudAddr, namespace, devCloudNamespace)
	if err != nil {
		return err
	}

	err = createClusterSecrets(clientset, clusterID, namespace)
	if err != nil {
		return err
	}

	return nil

}

func createCloudConfig(cloudAddr string, namespace string, devCloudNamespace string) error {
	// Attempt to delete an existing pl-cloud-config configmap.
	delCmd := exec.Command("kubectl", "delete", "configmap", "pl-cloud-config", "-n", namespace)
	_ = delCmd.Run()
	// devCloudNamespace implies we are running in a dev enivironment and we should attach to
	// vzconn in that namespace.
	if devCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNamespace)
	}
	// Create a new pl-cloud-config configmap.
	cloudAddrConf := fmt.Sprintf("--from-literal=PL_CLOUD_ADDR=%s", cloudAddr)
	createCmd := exec.Command("kubectl", "create", "configmap", "pl-cloud-config", cloudAddrConf, "-n", namespace)
	return createCmd.Run()
}

func createClusterSecrets(clientset *kubernetes.Clientset, clusterID string, namespace string) error {
	jwtSigningKey := make([]byte, 64)
	_, err := rand.Read(jwtSigningKey)
	if err != nil {
		return errors.New("Could not generate JWT signing key")
	}

	// Load clusterID and JWT signing key as a secret.
	k8s.DeleteSecret(clientset, namespace, "pl-cluster-secrets")
	err = k8s.CreateGenericSecretFromLiterals(clientset, namespace, "pl-cluster-secrets", map[string]string{
		"cluster-id":      clusterID,
		"jwt-signing-key": fmt.Sprintf("%x", jwtSigningKey),
	})
	if err != nil {
		return err
	}

	return nil
}
