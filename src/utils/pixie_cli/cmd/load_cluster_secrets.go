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
		cloudAddr, _ := cmd.Flags().GetString("cloud_addr")
		clusterID, _ := cmd.Flags().GetString("cluster_id")
		namespace, _ := cmd.Flags().GetString("namespace")

		kubeConfig := k8s.GetConfig()
		clientset := k8s.GetClientset(kubeConfig)

		err := LoadClusterSecrets(clientset, cloudAddr, clusterID, namespace)
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
}

// LoadClusterSecrets loads Vizier's secrets and configmap to the given namespace.
func LoadClusterSecrets(clientset *kubernetes.Clientset, cloudAddr string, clusterID string, namespace string) error {
	if clusterID == "" {
		return errors.New("cluster_id is required")
	}

	err := createCloudConfig(cloudAddr, namespace)
	if err != nil {
		return err
	}

	err = createClusterSecrets(clientset, clusterID, namespace)
	if err != nil {
		return err
	}

	return nil

}

func createCloudConfig(cloudAddr string, namespace string) error {
	// Attempt to delete an existing pl-cloud-config configmap.
	delCmd := exec.Command("kubectl", "delete", "configmap", "pl-cloud-config", "-n", namespace)
	_ = delCmd.Run()

	// Create a new pl-cloud-config configmap.
	createCmd := exec.Command("kubectl", "create", "configmap", "pl-cloud-config", fmt.Sprintf("--from-literal=PL_CLOUD_ADDR=%s", cloudAddr), "-n", namespace)
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
	k8s.CreateGenericSecretFromLiterals(clientset, namespace, "pl-cluster-secrets", map[string]string{
		"cluster-id":      clusterID,
		"jwt-signing-key": fmt.Sprintf("%x", jwtSigningKey),
	})

	return nil
}
