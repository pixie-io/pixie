package cmd

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/artifacts"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

func init() {
	GenerateYAMLs.Flags().StringP("base", "b", "",
		"Path to the tar containing the base Vizier YAMLs")
	viper.BindPFlag("base", GenerateYAMLs.Flags().Lookup("base"))
	GenerateYAMLs.MarkFlagRequired("base")

	GenerateYAMLs.Flags().StringP("out", "o", "", "The output path")
	viper.BindPFlag("out", GenerateYAMLs.Flags().Lookup("out"))
	GenerateYAMLs.MarkFlagRequired("out")

	GenerateYAMLs.Flags().StringP("version", "v", "", "The version string for the YAMLs")
	viper.BindPFlag("version", GenerateYAMLs.Flags().Lookup("version"))
	GenerateYAMLs.MarkFlagRequired("version")

	GenerateYAMLs.Flags().StringP("namespace", "n", "pl", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", GenerateYAMLs.Flags().Lookup("namespace"))
}

// GenerateYAMLs is the 'generate-yamls' command. It's used to create the templated YAMLs used for deploying
// Pixie through Helm.
var GenerateYAMLs = &cobra.Command{
	Use:   "generate-yamls",
	Short: "Create templated YAMLs for deploying Vizier",
	// Since this is an internal command we will hide it.
	Hidden: true,
	Run: func(cmd *cobra.Command, args []string) {
		baseTar, _ := cmd.Flags().GetString("base")
		outputPath, _ := cmd.Flags().GetString("out")
		version, _ := cmd.Flags().GetString("version")
		ns, _ := cmd.Flags().GetString("namespace")

		kubeConfig := k8s.GetConfig()
		clientset := k8s.GetClientset(kubeConfig)

		templatedYAMLs, err := artifacts.GenerateTemplatedDeployYAMLsWithTar(clientset, baseTar, version, ns)
		if err != nil {
			log.WithError(err).Fatal("failed to generate templated deployment YAMLs")
		}

		if err := artifacts.ExtractYAMLs(templatedYAMLs, outputPath, "pixie_yamls", artifacts.MultiFileExtractYAMLFormat); err != nil {
			log.WithError(err).Fatal("failed to extract deployment YAMLs")
		}
	},
}
