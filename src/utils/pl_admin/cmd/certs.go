package cmd

import (
	"github.com/spf13/cobra"

	"pixielabs.ai/pixielabs/src/utils/pl_admin/cmd/certs"
)

// NewCmdInstallCerts creates a new "install-certs" command.
func NewCmdInstallCerts() *cobra.Command {
	return &cobra.Command{
		Use:   "install-certs",
		Short: "Generates the proper server and client certs",
		Run: func(cmd *cobra.Command, args []string) {
			certPath, _ := cmd.Flags().GetString("cert_path")
			caCert, _ := cmd.Flags().GetString("ca_cert")
			caKey, _ := cmd.Flags().GetString("ca_key")
			namespace, _ := cmd.Flags().GetString("namespace")
			bitsize, _ := cmd.Flags().GetInt("bit_size")
			certs.InstallCerts(certPath, caCert, caKey, namespace, bitsize)
		},
	}
}
