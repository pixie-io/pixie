package cmd

import (
	"context"
	"errors"
	"fmt"
	"os"
	"time"

	"github.com/blang/semver"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/auth"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/update"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	version "pixielabs.ai/pixielabs/src/shared/version/go"
	utils2 "pixielabs.ai/pixielabs/src/utils"
)

func init() {
	UpdateCmd.AddCommand(CLIUpdateCmd)
	UpdateCmd.AddCommand(VizierUpdateCmd)

	CLIUpdateCmd.Flags().StringP("use_version", "v", "", "Select a specific version to install")
	_ = CLIUpdateCmd.Flags().MarkHidden("use_version")
	_ = viper.BindPFlag("use_version", CLIUpdateCmd.Flags().Lookup("use_version"))

	VizierUpdateCmd.Flags().BoolP("redeploy_etcd", "e", false, "Whether or not to redeploy etcd during the update")
	_ = viper.BindPFlag("redeploy_etcd", VizierUpdateCmd.Flags().Lookup("redeploy_etcd"))
	VizierUpdateCmd.Flags().StringP("cluster", "c", "", "Run only on selected cluster")

}

// UpdateCmd is the "update" sub-command of the CLI.
var UpdateCmd = &cobra.Command{
	Use:   "update",
	Short: "Update Pixie/CLI",
	Run: func(cmd *cobra.Command, args []string) {
		utils.Info("Nothing here... Please execute one of the subcommands")
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
		redeployEtcd := viper.GetBool("redeploy_etcd")

		clusterID := uuid.Nil
		clusterStr, _ := cmd.Flags().GetString("cluster")
		if len(clusterStr) > 0 {
			u, err := uuid.FromString(clusterStr)
			if err != nil {
				utils.WithError(err).Error("Failed to parse cluster argument")
				os.Exit(1)
			}
			clusterID = u
		}

		// Get grpc connection to cloud.
		cloudConn, err := utils.GetCloudClientConnection(cloudAddr)
		if err != nil {
			// Keep this as a log.Fatal() as opposed to using the cliLog, because it
			// is an unexpected error that Sentry should catch.
			log.Fatalln(err)
		}

		clusterInfo, err := getClusterForUpgrade(cloudConn, clusterID)
		if err != nil {
			// Keep this as a log.Fatal() as opposed to using the cliLog, because it
			// is an unexpected error that Sentry should catch.
			log.WithError(err).Fatal("Failed to fetch cluster information")
		}

		clusterID = utils2.UUIDFromProtoOrNil(clusterInfo.ID)
		status := clusterInfo.Status
		if status == cloudapipb.CS_DISCONNECTED {
			utils.Errorf("Cluster must be connected to update. status=%v", status)
			os.Exit(1)
		}

		if len(versionString) == 0 {
			// Fetch latest version.
			versionString, err = getLatestVizierVersion(cloudConn)
			if err != nil {
				// Keep this as a log.Fatal() as opposed to using the cliLog, because it
				// is an unexpected error that Sentry should catch.
				log.WithError(err).Fatal("Failed to fetch Vizier versions")
			}
		}

		if sv, err := semver.Parse(clusterInfo.VizierVersion); err == nil {
			svNew := semver.MustParse(versionString)
			if svNew.Compare(sv) < 0 {
				utils.Errorf("Cannot upgrade current version %s to requested older version %s",
					sv.String(), svNew.String())
				os.Exit(1)
			}
		}

		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Vizier Update Initiated",
			Properties: analytics.NewProperties().
				Set("cloud_addr", cloudAddr).
				Set("cluster_id", utils2.UUIDFromProtoOrNil(clusterInfo.ID)).
				Set("cluster_status", status.String()),
		})

		utils.Infof("Updating to version: %s", versionString)

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
		defer cancel()
		updateJobs := []utils.Task{
			newTaskWrapper("Initiating Update", func() error {
				return initiateUpdate(ctx, cloudConn, clusterID, versionString, redeployEtcd)
			}),
			newTaskWrapper("Wait for update", func() error {
				timer := time.NewTicker(5 * time.Second)
				timeout := time.NewTimer(5 * time.Minute)
				defer timer.Stop()
				defer timeout.Stop()
				for {
					select {
					case <-timer.C:
						clusterInfo, err := getClusterForUpgrade(cloudConn, clusterID)
						if err != nil {
							return err
						}
						if clusterInfo.Status == cloudapipb.CS_HEALTHY {
							updateVersion, err := semver.Parse(versionString)
							if err != nil {
								return err
							}
							currentVersion, err := semver.Parse(clusterInfo.VizierVersion)
							if err != nil {
								return err
							}

							if currentVersion.Compare(updateVersion) == 0 {
								return nil
							}
						}
					case <-timeout.C:
						return errors.New("timeout waiting for update")
					}
				}
			}),
			newTaskWrapper("Wait for healthcheck", waitForHealthCheckTaskGenerator(cloudAddr, clusterID)),
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

			// Keep as log.Fatal which produces a Sentry error for this unexpected behavior
			// (as opposed to user error which shouldn't be tracked in Sentry)
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
				utils.Info("No updates available")
				return
			}

			if ok, err := updater.IsUpdatable(); !ok || err != nil {
				if err != nil {
					panic(err)
				}
				utils.Error("cannot perform update, it's likely the file is not in a writable path.")
				os.Exit(1)
				// TODO(zasgar): Provide a means to update this as well.
			}
			selectedVersion = versions[0]
			if len(selectedVersion) == 0 {
				return
			}
		}

		utils.Infof("Updating to version: %s", selectedVersion)
		mustInstallVersion(updater, selectedVersion)
	},
}

func mustInstallVersion(u *update.CLIUpdater, v string) {
	err := u.UpdateSelf(v)
	if err != nil {
		utils.Error("Failed to apply update")
		panic(err)
	}
	utils.Info("Update completed successfully")
}

func getClusterForUpgrade(conn *grpc.ClientConn, c uuid.UUID) (*cloudapipb.ClusterInfo, error) {
	client := cloudapipb.NewVizierClusterInfoClient(conn)

	creds, err := auth.MustLoadDefaultCredentials()
	if err != nil {
		return nil, err
	}
	ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", creds.Token))
	resp, err := client.GetClusterInfo(ctx, &cloudapipb.GetClusterInfoRequest{})
	if err != nil {
		return nil, err
	}
	if len(resp.Clusters) == 0 {
		return nil, errors.New("no clusters available")
	}
	if c == uuid.Nil {
		return resp.Clusters[0], nil
	}
	// Try to find the cluster by ID.
	for _, cluster := range resp.Clusters {
		if utils2.UUIDFromProtoOrNil(cluster.ID) == c {
			return cluster, nil
		}
	}
	return nil, errors.New("selected cluster not found")
}
