package cmd

import (
	"context"
	"fmt"
	"os"
	"sort"
	"strings"
	"time"

	"github.com/blang/semver"
	"github.com/dustin/go-humanize"
	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/script"
	cliLog "pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/vizier"
	"pixielabs.ai/pixielabs/src/utils"
)

func init() {
	GetCmd.PersistentFlags().StringP("output", "o", "", "Output format: one of: json|proto")

	GetPEMsCmd.Flags().BoolP("all-clusters", "d", false, "Run script across all clusters")
	GetPEMsCmd.Flags().StringP("cluster", "c", "", "Run only on selected cluster")
	GetPEMsCmd.Flags().MarkHidden("all-clusters")

	GetCmd.AddCommand(GetPEMsCmd)
	GetCmd.AddCommand(GetViziersCmd)
}

// GetPEMsCmd is the "get pem" command.
var GetPEMsCmd = &cobra.Command{
	Use:     "pems",
	Aliases: []string{"agents", "pem"},
	Short:   "Get information about running pems",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)
		br := mustCreateBundleReader()
		execScript := br.MustGetScript(script.AgentStatusScript)

		allClusters, _ := cmd.Flags().GetBool("all-clusters")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		clusterID := uuid.FromStringOrNil(selectedCluster)
		var err error
		if !allClusters && clusterID == uuid.Nil {
			clusterID, err = vizier.GetCurrentOrFirstHealthyVizier(cloudAddr)
			if err != nil {
				cliLog.WithError(err).Error("Could not fetch healthy vizier")
				os.Exit(1)
			}
		}

		conns := vizier.MustConnectDefaultVizier(cloudAddr, allClusters, clusterID)

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()
		if err := vizier.RunScriptAndOutputResults(ctx, conns, execScript, format); err != nil {
			cliLog.Errorf("Script failed: %s", vizier.FormatErrorMessage(err))
			os.Exit(1)
		}
	},
}

// GetViziersCmd is the "get viziers" command.
var GetViziersCmd = &cobra.Command{
	Use:     "viziers",
	Aliases: []string{"clusters"},
	Short:   "Get information about registered viziers",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)

		l, err := vizier.NewLister(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to create Vizier lister")
		}
		vzs, err := l.GetViziersInfo()
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatalln("Failed to get vizier information")
		}

		sort.Slice(vzs, func(i, j int) bool { return vzs[i].ClusterName < vzs[j].ClusterName })

		w := components.CreateStreamWriter(format, os.Stdout)
		defer w.Finish()
		w.SetHeader("viziers", []string{"ClusterName", "ID", "K8s Version", "Vizier Version", "Status", "LastHeartbeat", "Passthrough"})

		for _, vz := range vzs {
			passthrough := false
			if vz.Config != nil {
				passthrough = vz.Config.PassthroughEnabled
			}
			var lastHeartbeat interface{}
			lastHeartbeat = vz.LastHeartbeatNs
			if format == "" || format == "table" {
				if vz.LastHeartbeatNs >= 0 {
					lastHeartbeat = humanize.Time(
						time.Unix(0,
							time.Since(time.Unix(0, vz.LastHeartbeatNs)).Nanoseconds()))
				}
			}
			sb := strings.Builder{}

			// Parse the version to pretty print it.
			if sv, err := semver.Parse(vz.VizierVersion); err == nil {
				sb.WriteString(fmt.Sprintf("%d.%d.%d", sv.Major, sv.Minor, sv.Patch))
				for idx, pre := range sv.Pre {
					if idx == 0 {
						sb.WriteString("-")
					} else {
						sb.WriteString(".")
					}
					sb.WriteString(pre.String())
				}
			}
			_ = w.Write([]interface{}{vz.ClusterName, utils.UUIDFromProtoOrNil(vz.ID), vz.ClusterVersion, sb.String(),
				vz.Status, lastHeartbeat, passthrough})
		}
	},
}

// GetCmd is the "get" command.
var GetCmd = &cobra.Command{
	Use:   "get",
	Short: "Get information about cluster/edge modules",
}
