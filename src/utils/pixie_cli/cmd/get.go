package cmd

import (
	"context"
	"strings"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"
)

func init() {
	GetCmd.Flags().StringP("output", "o", "", "Output format: one of: json|proto")
}

// GetCmd is the "get" command.
var GetCmd = &cobra.Command{
	Use:   "get",
	Short: "Get information about cluster/edge modules",
	Run: func(cmd *cobra.Command, args []string) {
		// TODO(zasgar): Improvement needed after we spec out multiple vizier/agents support.
		// Placeholder function until we spec out vizier/agent listing.
		if len(args) != 1 || args[0] != "pem" {
			log.Fatalln("only a single argument pem is allowed")
		}

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
