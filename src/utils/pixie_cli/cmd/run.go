package cmd

import (
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"strings"

	"github.com/gogo/protobuf/jsonpb"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

func init() {
	RunCmd.Flags().StringP("output", "o", "", "Output format: one of: json|proto")
	RunCmd.Flags().StringP("file", "f", "", "Script file, specify - for STDIN")
}

// RunCmd is the "query" command.
var RunCmd = &cobra.Command{
	Use:   "run",
	Short: "Execute a script",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)

		scriptFile, _ := cmd.Flags().GetString("file")
		q, err := getScriptString(scriptFile)
		if err != nil {
			log.WithError(err).Fatal("Failed to get query string")
		}

		// TODO(zasgar): Refactor this when we change to the new API to make analytics cleaner.
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Started",
			Properties: analytics.NewProperties().
				Set("scriptString", q),
		})

		v := mustConnectDefaultVizier(cloudAddr)

		res, err := v.ExecuteScript(q)
		if err != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Script Execution Failed",
				Properties: analytics.NewProperties().
					Set("scriptString", q),
			})
			log.WithError(err).Fatal("Failed to execute query")
		}
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Success",
			Properties: analytics.NewProperties().
				Set("scriptString", q),
		})
		mustFormatQueryResults(res, format)
	},
}

func mustFormatQueryResults(res *querybrokerpb.VizierQueryResponse, format string) {
	var err error = nil
	switch format {
	case "json":
		{
			err = formatAsJSON(res)
		}
	case "pb":
		var b []byte
		b, err = res.Marshal()
		if err == nil {
			fmt.Printf("%s", string(b))
		}
	case "pbtxt":
		fmt.Print(res.String())
	default:
		formatResultsAsTable(res)
	}

	if err != nil {
		log.WithError(err).Fatalln("Failed to print results")
	}
}

func formatAsJSON(r *querybrokerpb.VizierQueryResponse) error {
	m := jsonpb.Marshaler{}
	return m.Marshal(os.Stdout, r)
}

func formatResultsAsTable(r *querybrokerpb.VizierQueryResponse) {
	t := components.NewTableRenderer()
	queryResult := r.QueryResult
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

func getScriptString(path string) (string, error) {
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
		return "", errors.New("script string is empty")
	}
	return string(qb), nil
}
