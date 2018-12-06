package commands

import (
	"os"
	"path"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	bccinfo "pixielabs.ai/pixielabs/platform-dependent/smoke-test/bccinfo"
	generatesysteminfo "pixielabs.ai/pixielabs/platform-dependent/smoke-test/generate-system-info"
	systemchecks "pixielabs.ai/pixielabs/platform-dependent/smoke-test/system-checks"
)

// PixieCmd is a cobra CLI utility to run smoke-tests on client machines to determine validity for agents.
var (
	// output location to store the host info as a protobuf text file.
	output string
	// check host system viability for Pixie agents by running targeted tests.
	hostCheck bool
	// gather information about the host system.
	genInfo bool
	// check output of the test bcc script, CPUDistribution
	bccCheck bool
	// the location of the bcc script output
	bccOutput string
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
			executeCommand()
		},
	}
)

func init() {
	PixieCmd.SetVersionTemplate("Pixie smoke-test cli version: {{.Version}}\n")

	// Flags with default values.
	PixieCmd.Flags().StringVarP(&output, "out", "o", "",
		"Output file (absolute path with file name) to store host system information")
	PixieCmd.Flags().BoolVarP(&hostCheck, "hostCheck", "c", false,
		"Run checks on host system to validate Pixie agent compatibility")
	PixieCmd.Flags().BoolVarP(&genInfo, "gen_info", "g", false,
		"Generate host system information and write a protobuf text file")
	PixieCmd.Flags().BoolVarP(&bccCheck, "bcc_check", "b", false,
		"Generate host system information and write a protobuf text file")
	PixieCmd.Flags().StringVarP(&bccOutput, "bcc_output", "f", "",
		"Read in the output file at this path")

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)
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
			log.Info("Output file created: ", outputFileName)
			log.Info("Generating system information\n")
			pbSystemInfo, pbHostInfo := generatesysteminfo.InfoForOS(fileHandle)
			log.Info("Done generating system information\n")

			if hostCheck {
				// Run the host checks.
				systemchecks.RunChecker(fileHandle, pbSystemInfo, pbHostInfo)
			}

			if bccCheck {
				bccinfo.RunChecker(fileHandle, bccOutput)
			}

			// Close the output file
			if err := fileHandle.Close(); err != nil {
				log.WithError(err).Fatalf("Cannot close file")
			}
		}
	}

}

func checkCommandFlags(args []string) {
	if output != "" && !genInfo {
		log.Fatalf("Need to specify --gen_info (-g) option to generate host system information")
	}

	if hostCheck && !genInfo {
		log.Fatalf("Need to specify --gen_info (-g) option along with --host_check (-c)")
	}

	if bccCheck && bccOutput == "" {
		log.Fatalf("If specifying --bcc_check (-b) you must specify --bcc_output (-f)")
	}
}
