package cmd

import (
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"strings"

	"github.com/gogo/protobuf/jsonpb"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

func init() {
	QueryCmd.Flags().StringP("output", "o", "", "Output format: one of: json|proto")
	viper.BindPFlag("output", QueryCmd.Flags().Lookup("output"))
	QueryCmd.Flags().StringP("file", "f", "", "Query file, specify - for STDIN")
	viper.BindPFlag("file", QueryCmd.Flags().Lookup("file"))

}

// QueryCmd is the "query" command.
var QueryCmd = &cobra.Command{
	Use:   "query",
	Short: "Execute a query",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format := viper.GetString("output")
		format = strings.ToLower(format)

		queryFile := viper.GetString("file")
		q, err := getQueryString(queryFile)
		if err != nil {
			log.WithError(err).Fatal("Failed to get query string")
		}

		v := mustConnectDefaultVizier(cloudAddr)

		res, err := v.ExecuteQuery(q)
		if err != nil {
			log.WithError(err).Fatal("Failed to execute query")
		}

		err = nil
		switch format {
		case "json":
			{
				err = formatAsJSON(res)
			}
		case "pb":
			fmt.Print(res)
		case "pbtxt":
			fmt.Print(res.String())
		default:
			formatResultsAsTable(res)
		}

		if err != nil {
			log.WithError(err).Fatalln("Failed to print results")
		}
	},
}

func formatAsJSON(r *querybrokerpb.VizierQueryResponse) error {
	m := jsonpb.Marshaler{}
	return m.Marshal(os.Stdout, r)
}

func formatResultsAsTable(r *querybrokerpb.VizierQueryResponse) {
	t := components.NewTableRenderer()
	fmt.Printf("Got %d Response(s)\n", len(r.Responses))
	for _, agentResp := range r.Responses {
		agentUUID := uuid.FromStringOrNil(string(agentResp.AgentID.Data))

		fmt.Printf("Agent ID: %s\n", agentUUID)
		queryResult := agentResp.Response.QueryResult
		execStats := queryResult.ExecutionStats
		timingStats := queryResult.TimingInfo
		bytesProcessed := float64(execStats.BytesProcessed)
		execTimeNS := float64(timingStats.ExecutionTimeNs)
		for _, table := range queryResult.Tables {
			t.RenderTable(table)
		}
		fmt.Printf("Compilation Time: %.2f ms\n", float64(timingStats.CompilationTimeNs)/1.0e6)
		fmt.Printf("Execution Time: %.2f ms\n", execTimeNS/1.0e6)
		fmt.Printf("Bytes processed: %.2f KB\n", bytesProcessed/1024)
	}
}

func getQueryString(path string) (string, error) {
	var qb []byte
	var err error
	if path == "-" {
		// Read from STDIN.
		qb, err = ioutil.ReadAll(os.Stdin)
		if err != nil {
			panic(err)
		}
	} else {
		r, err := os.Open(path)
		if err != nil {
			return "", err
		}

		qb, err = ioutil.ReadAll(r)
		if err != nil {
			return "", err
		}
	}

	if len(qb) == 0 {
		return "", errors.New("query string is empty")
	}
	return string(qb), nil
}
