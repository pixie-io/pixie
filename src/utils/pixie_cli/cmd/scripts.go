package cmd

import (
	"os"

	"github.com/alecthomas/chroma/quick"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

func init() {
	ScriptCmd.AddCommand(ScriptListCmd)
	ScriptCmd.AddCommand(ScriptShowCmd)
	// Allow run as an alias to keep scripts self contained.
	ScriptCmd.AddCommand(RunCmd)

	ScriptCmd.PersistentFlags().StringP("bundle", "b", "", "Path/URL to bundle file")
	viper.BindPFlag("bundle", ScriptCmd.PersistentFlags().Lookup("bundle"))

	ScriptListCmd.Flags().StringP("output", "o", "", "Output format: one of: json|table")
	viper.BindPFlag("output_format", ScriptListCmd.Flags().Lookup("output"))
}

// ScriptCmd is the "script" command.
var ScriptCmd = &cobra.Command{
	Use:     "script",
	Short:   "Get information about pre-registered scripts",
	Aliases: []string{"scripts"},
}

// ScriptListCmd is the "script list" command.
var ScriptListCmd = &cobra.Command{
	Use:     "list",
	Short:   "List pre-registered pxl scripts",
	Aliases: []string{"scripts"},
	Run: func(cmd *cobra.Command, args []string) {
		bundle := viper.GetString("bundle")
		if bundle == "" {
			bundle = defaultBundleFile
		}
		listBundleScripts(bundle, viper.GetString("output_format"))
	},
}

// ScriptShowCmd is the "script show" command.
var ScriptShowCmd = &cobra.Command{
	Use:     "show",
	Short:   "Dumps out the string for a particular pxl script",
	Args:    cobra.ExactArgs(1),
	Aliases: []string{"scripts"},
	Run: func(cmd *cobra.Command, args []string) {
		bundle := viper.GetString("bundle")
		if bundle == "" {
			bundle = defaultBundleFile
		}
		scriptName := args[0]
		script, _, err := getScriptFromBundle(bundle, scriptName)
		if err != nil {
			log.WithError(err).Fatal("Failed to get script information")
		}
		err = quick.Highlight(os.Stdout, script, "python3", "terminal16m", "monokai")
		if err != nil {
			panic(err)
		}
	},
}
