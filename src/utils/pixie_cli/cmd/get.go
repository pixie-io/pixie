package cmd

import (
	"context"
	"os"
	"strings"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"
)

func init() {
	GetCmd.PersistentFlags().StringP("output", "o", "", "Output format: one of: json|proto")

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
		v := mustConnectDefaultVizier(cloudAddr)
		q := `px.display(px.GetAgentStatus())`

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()

		resp, err := v.ExecuteScriptStream(ctx, q)
		if err != nil {
			log.WithError(err).Fatal("Failed to execute query")
		}

		tw := vizier.NewVizierStreamOutputAdapter(ctx, resp, format)
		tw.Finish()
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
			log.WithError(err).Fatal("Failed to create Vizier lister")
		}
		vzs, err := l.GetViziersInfo()
		if err != nil {
			log.WithError(err).Fatalln("Failed to get vizier information")
		}

		w := components.CreateStreamWriter(format, os.Stdout)
		defer w.Finish()
		w.SetHeader("viziers", []string{"ID", "Status", "LastHeartbeat", "Passthrough"})
		for _, vz := range vzs {
			passthrough := false
			if vz.Config != nil {
				passthrough = vz.Config.PassthroughEnabled
			}
			_ = w.Write([]interface{}{utils.UUIDFromProtoOrNil(vz.ID), vz.Status, vz.LastHeartbeatNs, passthrough})
		}
	},
}

// GetCmd is the "get" command.
var GetCmd = &cobra.Command{
	Use:   "get",
	Short: "Get information about cluster/edge modules",
}
