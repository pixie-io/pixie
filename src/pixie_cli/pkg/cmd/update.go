/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package cmd

import (
	"context"
	"errors"
	"strings"
	"time"

	"github.com/blang/semver"
	"github.com/gofrs/uuid"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/update"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	version "px.dev/pixie/src/shared/goversion"
	utils2 "px.dev/pixie/src/utils"
)

func init() {
	UpdateCmd.AddCommand(CLIUpdateCmd)
	UpdateCmd.AddCommand(VizierUpdateCmd)

	CLIUpdateCmd.Flags().StringP("cli_version", "v", "", "Select a specific version to install")
	CLIUpdateCmd.Flags().MarkHidden("cli_version")

	VizierUpdateCmd.Flags().StringP("vizier_version", "v", "", "Select a specific version to install")
	VizierUpdateCmd.Flags().MarkHidden("vizier_version")
	VizierUpdateCmd.Flags().BoolP("redeploy_etcd", "e", false, "Whether or not to redeploy etcd during the update")
	VizierUpdateCmd.Flags().StringP("cluster", "c", "", "Run only on selected cluster")
}

// UpdateCmd is the "update" sub-command of the CLI.
var UpdateCmd = &cobra.Command{
	Use:   "update",
	Short: "Update Pixie/CLI",
	Run: func(cmd *cobra.Command, args []string) {
		utils.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
	},
}

// VizierUpdateCmd is the command used to update Vizier.
var VizierUpdateCmd = &cobra.Command{
	Use:     "vizier",
	Aliases: []string{"platform", "pixie"},
	Short:   "Run updates of Pixie Platform",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("vizier_version", cmd.Flags().Lookup("vizier_version"))
		viper.BindPFlag("redeploy_etcd", cmd.Flags().Lookup("redeploy_etcd"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		versionString := viper.GetString("vizier_version")
		cloudAddr := viper.GetString("cloud_addr")
		redeployEtcd := viper.GetBool("redeploy_etcd")

		clusterID := uuid.Nil
		clusterStr, _ := cmd.Flags().GetString("cluster")
		if len(clusterStr) > 0 {
			u, err := uuid.FromString(clusterStr)
			if err != nil {
				utils.WithError(err).Fatal("Failed to parse cluster argument")
			}
			clusterID = u
		}

		// Get grpc connection to cloud.
		cloudConn, err := utils.GetCloudClientConnection(cloudAddr)
		if err != nil {
			// Keep this as a log.Fatal() as opposed to using the CLI logger, because it
			// is an unexpected error that Sentry should catch.
			log.Fatalln(err)
		}

		if clusterID == uuid.Nil {
			clusterID, err = vizier.GetCurrentOrFirstHealthyVizier(cloudAddr)
			if err != nil {
				utils.WithError(err).Fatal("Failed to select cluster")
			}
		}
		clusterInfo, err := vizier.GetVizierInfo(cloudAddr, clusterID)
		if err != nil {
			utils.WithError(err).Fatalf("Failed to get info for cluster: %s", clusterID.String())
		}

		utils.Infof("Updating Pixie on the following cluster: %s", clusterInfo.ClusterName)
		clusterOk := components.YNPrompt("Is the cluster correct?", true)
		if !clusterOk {
			utils.Error("Cluster is not correct. Aborting.")
			return
		}

		if len(versionString) == 0 {
			// Fetch latest version.
			versionString, err = getLatestVizierVersion(cloudConn)
			if err != nil {
				// Keep this as a log.Fatal() as opposed to using the CLI logger, because it
				// is an unexpected error that Sentry should catch.
				log.WithError(err).Fatal("Failed to fetch Vizier versions")
			}
		}

		if sv, err := semver.Parse(clusterInfo.VizierVersion); err == nil {
			svNew := semver.MustParse(versionString)
			if svNew.Compare(sv) < 0 {
				utils.Fatalf("Cannot upgrade current version %s to requested older version %s",
					sv.String(), svNew.String())
			}
		}

		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Vizier Update Initiated",
			Properties: analytics.NewProperties().
				Set("cloud_addr", cloudAddr).
				Set("cluster_id", utils2.UUIDFromProtoOrNil(clusterInfo.ID)).
				Set("cluster_status", clusterInfo.Status.String()),
		})

		utils.Infof("Updating to version: %s", versionString)

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
		defer cancel()
		updateJobs := []utils.Task{
			newTaskWrapper("Initiating update", func() error {
				return initiateUpdate(ctx, cloudConn, clusterID, versionString, redeployEtcd)
			}),
			newTaskWrapper("Wait for update to complete (this may take a few minutes)", func() error {
				timer := time.NewTicker(5 * time.Second)
				timeout := time.NewTimer(5 * time.Minute)
				defer timer.Stop()
				defer timeout.Stop()
				for {
					select {
					case <-timer.C:
						clusterInfo, err := vizier.GetVizierInfo(cloudAddr, clusterID)
						if err != nil {
							return err
						}
						if clusterInfo.Status == cloudpb.CS_HEALTHY {
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
		viper.BindPFlag("cli_version", cmd.Flags().Lookup("cli_version"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		selectedVersion := viper.GetString("cli_version")

		updater := update.NewCLIUpdater(viper.GetString("cloud_addr"))
		currVersion := version.GetVersion()
		if len(selectedVersion) == 0 {
			// Not specified try to get available.
			versions, err := updater.GetAvailableVersions(currVersion.Semver())
			if err != nil {
				utils.WithError(err).Fatal("Cannot determine new versions to update to.")
			}
			if len(versions) == 0 {
				utils.Info("No updates available")
				return
			}

			selectedVersion = versions[0]
			if len(selectedVersion) == 0 {
				return
			}
		}

		if ok, err := updater.IsUpdatable(); !ok || err != nil {
			utils.Fatal("Cannot perform update, it's likely the file is not in a writable path.")
			// TODO(zasgar): Provide a means to update this as well.
		}

		if strings.Contains(strings.ToLower(currVersion.Builder()), "homebrew") {
			continueUpdate := components.YNPrompt(`Homebrew installation detected. Please use homebrew to update the cli.
Update anyway?`, false)
			if !continueUpdate {
				utils.Error("Update cancelled.")
				return
			}
		}

		if !strings.Contains(strings.ToLower(currVersion.Builder()), "jenkins") {
			continueUpdate := components.YNPrompt(`Uncommon CLI installation.
We recommend rebuilding/updating the CLI using the same method as the initial install.
Update anyway?`, false)
			if !continueUpdate {
				utils.Error("Update cancelled.")
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
		utils.WithError(err).Fatal("Failed to apply update.")
	}
	utils.Info("Update completed successfully.")
}
