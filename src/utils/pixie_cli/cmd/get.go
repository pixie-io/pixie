package cmd

import (
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/olekukonko/tablewriter"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
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

		res, err := v.GetAgentInfo()
		if err != nil {
			log.WithError(err).Fatal("Failed to get agent info")
		}

		err = nil
		switch format {
		case "json":
			err = formatAgentResultsAsJSON(res)
		case "pb":
			fmt.Print(res)
		case "pbtxt":
			fmt.Print(res.String())
		default:
			formatAgentResultsAsTable(res)
		}

		if err != nil {
			log.WithError(err).Fatalln("Failed to print results")
		}
	},
}

func formatAgentResultsAsJSON(r *querybrokerpb.AgentInfoResponse) error {
	m := jsonpb.Marshaler{}
	return m.Marshal(os.Stdout, r)
}

func formatAgentResultsAsTable(r *querybrokerpb.AgentInfoResponse) {
	fmt.Printf("Number of agents: %d\n", len(r.Info))

	table := tablewriter.NewWriter(os.Stdout)
	table.SetHeader([]string{"AgentID", "Hostname", "Last Heartbeat (seconds)", "State"})
	for _, agentInfo := range r.Info {
		id := uuid.FromStringOrNil(string(agentInfo.Info.AgentID.Data))
		hbTime := time.Unix(0, agentInfo.LastHeartbeatNs)
		currentTime := time.Now()
		hbInterval := currentTime.Sub(hbTime).Seconds()
		table.Append([]string{id.String(),
			agentInfo.Info.HostInfo.Hostname,
			fmt.Sprintf("%.2f", hbInterval),
			agentInfo.State.String(),
		})
	}
	table.Render()
}
