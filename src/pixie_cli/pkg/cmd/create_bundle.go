package cmd

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/script"
)

func init() {
	CreateBundle.Flags().StringArrayP("base", "b", []string{"px"},
		"The base path(s) to use. for creating script bundles")
	viper.BindPFlag("base", CreateBundle.Flags().Lookup("base"))

	CreateBundle.Flags().StringArrayP("search_path", "s", []string{},
		"The paths to search for the pxl files")
	viper.BindPFlag("search_path", CreateBundle.Flags().Lookup("search_path"))
	CreateBundle.MarkFlagRequired("search_path")

	CreateBundle.Flags().StringP("out", "o", "-", "The output file")
	viper.BindPFlag("out", CreateBundle.Flags().Lookup("out"))
}

// CreateBundle is the 'create-bundle' command. It's used to create a script bundle that can be used by the UI/CLI.
// This is a temporary command until we have proper script persistence.
var CreateBundle = &cobra.Command{
	Use:   "create-bundle",
	Short: "Create a bundle for scripts",
	// Since this is an internal command we will hide it.
	Hidden: true,
	Run: func(cmd *cobra.Command, args []string) {
		basePaths, _ := cmd.Flags().GetStringArray("base")
		searchPaths, _ := cmd.Flags().GetStringArray("search_path")

		out, _ := cmd.Flags().GetString("out")
		b := script.NewBundleWriter(searchPaths, basePaths)
		err := b.Write(out)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to create bundle")
		}
	},
}
