package cmd

import (
	"io/ioutil"
	"path"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"

	"pixielabs.ai/pixielabs/src/utils/pl_admin/cmd/k8s"
)

// NewCmdDownloadCA creates a new "download-ca" command.
func NewCmdDownloadCA() *cobra.Command {
	return &cobra.Command{
		Use:   "download-ca",
		Short: "Downloads Vizier's CA to the specified path",
		Run: func(cmd *cobra.Command, args []string) {
			caPath, _ := cmd.Flags().GetString("ca_path")
			namespace, _ := cmd.Flags().GetString("namespace")
			downloadCA(caPath, namespace)
		},
	}
}

func downloadCA(caDir string, namespace string) {
	if caDir == "" {
		log.Fatal("ca_path flag is required")
	}

	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)
	secret := k8s.GetSecret(clientset, namespace, "service-tls-certs")

	ca := secret.Data["ca.crt"]

	caPath := path.Join(caDir, "ca.crt")

	err := ioutil.WriteFile(caPath, ca, 0644)
	if err != nil {
		log.Fatal("Failed to write CA to " + caPath)
	}

	log.Infof("Vizier CA downloaded to %s", caPath)
}
