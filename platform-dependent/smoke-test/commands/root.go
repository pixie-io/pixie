package commands

import (
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"os"
	"path"
	"pixielabs.ai/pixielabs/platform-dependent/smoke-test/generate-system-info"
)

// PixieCmd is a cobra CLI utility to run smoke-tests on client machines to determine validity for agents.
var (
	// output location to store the host info as a protobuf text file.
	output string
	// check host system viability for Pixie agents by running targeted tests.
	check bool
	// gather information about the host system.
	genInfo bool
	// Default output filename
	defaultOutputFilename = "host-system-info.pbtxt"

	// smoke-test command.
	PixieCmd = &cobra.Command{
		Use:     "smoke-test",
		Version: `0.1`,
		Short:   "smoke-test is a utility to validate compute environment to run pixie agents",
		Long: `A command line utility to validate whether ` +
			`pixie agents can run successfully to generate system logs and network usage.`,
		PreRun: func(cmd *cobra.Command, args []string) {
			checkCommandFlags(args)
		},
		Run: func(cmd *cobra.Command, args []string) {
			cmd.HelpFunc()(cmd, args)
			executeCommand()
		},
	}
)

func init() {
	PixieCmd.SetVersionTemplate("Pixie smoke-test cli version: {{.Version}}\n")

	// Flags with default values.
	PixieCmd.Flags().StringVarP(&output, "out", "o", "",
		"Output file (absolute path with file name) to store host system information")
	PixieCmd.Flags().BoolVarP(&check, "check", "c", false,
		"Run checks on host system to validate Pixie agent compatibility")
	PixieCmd.Flags().BoolVarP(&genInfo, "genInfo", "g", false,
		"Generate host system information and write a protobuf text file")
}

func executeCommand() {

	var outputFileName string
	// Gather system information if the info flag has been set.
	if genInfo {
		// if --output is passed, then set the output.
		if output != "" {
			outputFileName = output
		} else {
			if outdir, err := os.Getwd(); err != nil {
				log.WithError(err).Fatalf("Cannot find output directory")
			} else {
				outputFileName = path.Join(outdir, defaultOutputFilename)
			}
		}

		if fileHandle, err := os.Create(outputFileName); err != nil {
			log.WithError(err).Fatalf("Cannot write to specified output file: %s", outputFileName)
		} else {
			log.Println("Output file created: ", outputFileName)
			log.Println("Generating system information\n")
			generatesysteminfo.InfoForOS(fileHandle)
			log.Println("Done generating system information\n")
			// TODO(kgandhi): Call the function to run the checker.
		}
	}

}

func checkCommandFlags(args []string) {
	if output != "" && genInfo == false {
		log.Fatalf("Need to specify --info option to generate host system information")
	}
}
