package cmd

import (
	"context"
	"fmt"
	"os"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/shared/version"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/update"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
)

func init() {
	UpdateCmd.AddCommand(CLIUpdateCmd)
	UpdateCmd.AddCommand(VizierUpdateCmd)

	CLIUpdateCmd.Flags().StringP("use_version", "v", "", "Select a specific version to install")
	_ = CLIUpdateCmd.Flags().MarkHidden("use_version")
	_ = viper.BindPFlag("use_version", CLIUpdateCmd.Flags().Lookup("use_version"))
}

// UpdateCmd is the "update" sub-command of the CLI.
var UpdateCmd = &cobra.Command{
	Use:   "update",
	Short: "Update Pixie/CLI",
	Run: func(cmd *cobra.Command, args []string) {
		log.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
		return
	},
}

// VizierUpdateCmd is the command used to update Vizier.
var VizierUpdateCmd = &cobra.Command{
	Use:     "vizier",
	Aliases: []string{"platform", "pixie"},
	Short:   "Run updates of Pixie Platform",
	PreRun: func(cmd *cobra.Command, args []string) {
		if e, has := os.LookupEnv("PL_VIZIER_VERSION"); has {
			viper.Set("use_version", e)
		}
	},
	Run: func(cmd *cobra.Command, args []string) {
		versionString := viper.GetString("use_version")
		cloudAddr := viper.GetString("cloud_addr")

		// Get grpc connection to cloud.
		cloudConn, err := getCloudClientConnection(cloudAddr)
		if err != nil {
			log.Fatalln(err)
		}

		status, clusterID, err := getClusterID(cloudAddr)
		if err != nil {
			log.WithError(err).Fatal("Failed to fetch cluster information")
		}

		if *status != cloudapipb.CS_HEALTHY {
			log.WithField("status", status.String()).Fatalf("Cluster must be in a healthy state to update")
		}

		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Vizier Update Initiated",
			Properties: analytics.NewProperties().
				Set("cloud_addr", cloudAddr).
				Set("cluster_id", clusterID).
				Set("cluster_status", status.String()),
		})

		if len(versionString) == 0 {
			// Fetch latest version.
			versionString, err = getLatestVizierVersion(cloudConn)
			if err != nil {
				log.WithError(err).Fatal("Failed to fetch Vizier versions")
			}
		}
		fmt.Printf("Updating to version: %s\n", versionString)

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
		defer cancel()
		updateJobs := []utils.Task{
			newTaskWrapper("Initiating Update", func() error {
				err := initiateUpdate(ctx, cloudConn, clusterID, versionString)
				if err != nil {
					return err
				}
				// TODO(zasgar): Fix this by doing a version check.
				time.Sleep(10 * time.Second)
				return nil
			}),
			newTaskWrapper("Wait for healthcheck", waitForHealthCheckTaskGenerator(cloudAddr)),
		}
		uj := utils.NewSerialTaskRunner(updateJobs)
		err = uj.RunAndMonitor()

		if err != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Vizier Update Failed",
				Properties: analytics.NewProperties().
					Set("cloud_addr", cloudAddr).
					Set("cluster_id", clusterID),
			})

			log.WithError(err).Fatal("Update failed")
		}

		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Vizier Update Complete",
			Properties: analytics.NewProperties().
				Set("cloud_addr", cloudAddr).
				Set("cluster_id", clusterID),
		})
	},
}

// CLIUpdateCmd is the cli subcommand of the "update" command.
var CLIUpdateCmd = &cobra.Command{
	Use:   "cli",
	Short: "Run updates of CLI",
	PreRun: func(cmd *cobra.Command, args []string) {
		if e, has := os.LookupEnv("PL_CLI_VERSION"); has {
			viper.Set("use_version", e)
		}
	},
	Run: func(cmd *cobra.Command, args []string) {
		selectedVersion := viper.GetString("use_version")

		updater := update.NewCLIUpdater(viper.GetString("cloud_addr"))
		if len(selectedVersion) == 0 {
			// Not specified try to get available.
			currentSemver := version.GetVersion().Semver()
			versions, err := updater.GetAvailableVersions(currentSemver)
			if err != nil {
				panic(err)
			}
			if len(versions) <= 0 {
				fmt.Println("No updates available")
				return
			}

			if ok, err := updater.IsUpdatable(); !ok || err != nil {
				if err != nil {
					panic(err)
				}
				fmt.Println("cannot perform update, it's likely the file is not in a writable path.")
				os.Exit(1)
				// TODO(zasgar): Provide a means to update this as well.
			}
			selectedVersion = versions[0]
			if len(selectedVersion) == 0 {
				return
			}
		}

		fmt.Printf("Updating to version: %s\n", selectedVersion)
		mustInstallVersion(updater, selectedVersion)
	},
}

func mustInstallVersion(u *update.CLIUpdater, v string) {
	err := u.UpdateSelf(v)
	if err != nil {
		fmt.Println("Failed to apply update")
		panic(err)
	}
	fmt.Println("Update completed successfully")
}
