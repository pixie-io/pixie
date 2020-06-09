package main

import (
	"context"
	"fmt"
	"os"
	"sort"
	"time"

	"github.com/olekukonko/tablewriter"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/shared/services"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"
)

var scriptBlacklist = []string{
	"px/http2_data",
}

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle.json"

func init() {
	pflag.Int("num_runs", 10, "number of times to run a script ")
	pflag.StringP("cloud_addr", "a", "withpixie.ai:443", "The address of Pixie Cloud")
	pflag.StringP("bundle", "b", defaultBundleFile, "The bundle file to use")
}

type execStats struct {
	Name                string
	NumRuns             int
	MeanBenchmarkTime   time.Duration
	MeanCompilationTime time.Duration
	MeanQBTime          time.Duration
	NumErrors           int
}

func createBundleReader(bundleFile string) (*script.BundleManager, error) {
	br, err := script.NewBundleManager(bundleFile)
	if err != nil {
		return nil, err
	}
	return br, nil
}

type execResults struct {
	externalExecTime time.Duration
	internalExecTime time.Duration
	compileTime      time.Duration
	scriptErr        error
}

func exec(v []*vizier.Connector, execScript *script.ExecutableScript) (*execResults, error) {

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	execRes := execResults{}
	start := time.Now()
	// Start running the streaming script.
	resp, err := vizier.RunScript(ctx, v, execScript)
	if err != nil {
		return nil, err
	}

	// Accumulate the streamed data and block until all data is received.
	tw := vizier.NewVizierStreamOutputAdapter(ctx, resp, "inmemory")
	err = tw.Finish()

	// Calculate the execution time.
	execRes.externalExecTime = time.Now().Sub(start)
	if err != nil {
		log.WithError(err).Infof("Error '%s' on '%s'", vizier.FormatErrorMessage(err), execScript.ScriptName)
	}

	// Store any error that comes up during execution.
	execRes.scriptErr = err

	// Get the exec stats collected during the stream accumulation.
	execStats, err := tw.ExecStats()
	if err != nil {
		return nil, err
	}
	execRes.internalExecTime = time.Duration(execStats.Timing.ExecutionTimeNs)
	execRes.compileTime = time.Duration(execStats.Timing.CompilationTimeNs)
	return &execRes, nil

}

func isBlacklist(script string) bool {
	for _, t := range scriptBlacklist {
		if script == t {
			return true
		}
	}
	return false
}

// ScriptExecData contains the data for a single executed script.
type ScriptExecData struct {
	// The execution time as recorded by QueryBroker.
	InternalExecTimes []time.Duration
	// The execution time including the round-trip from Vizier Client to QueryBroker.
	ExternalExecTimes []time.Duration
	// The time taken to compile the query.
	CompilationTimes []time.Duration
	// Errors that arise during execution. Does not include errors caused by failed connections to query broker.
	Errors []error
}

// ExecStatsWriter is the writer that takes in ExecData and outputs some other format.
type ExecStatsWriter interface {
	Write(*map[string]ScriptExecData) error
}

// stdoutTableWriter writes the execStats out to a table in stdout. Implements ExecStatsWriter.
type stdoutTableWriter struct {
}

func (s *stdoutTableWriter) Write(data *map[string]ScriptExecData) error {
	scriptStats := summarizeExecData(data)
	table := tablewriter.NewWriter(os.Stdout)
	table.SetHeader([]string{"Name", "Num Runs", "Mean Compilation Time", "Mean Internal Exec Time", "Mean External Exec Time", "Num Errors"})

	for _, v := range *scriptStats {
		table.Append([]string{
			v.Name,
			fmt.Sprintf("%v", v.NumRuns),
			fmt.Sprintf("%v", v.MeanCompilationTime),
			fmt.Sprintf("%v", v.MeanQBTime),
			fmt.Sprintf("%v", v.MeanBenchmarkTime),
			fmt.Sprintf("%v", v.NumErrors),
		})
	}
	table.Render()
	return nil
}

func summarizeExecData(data *map[string]ScriptExecData) *[]execStats {
	sorted := make([]string, 0)
	for name := range *data {
		sorted = append(sorted, name)
	}

	sort.Strings(sorted)

	scriptStats := make([]execStats, 0)
	for _, name := range sorted {
		d := (*data)[name]
		if len(d.InternalExecTimes) == 0 || len(d.CompilationTimes) == 0 || len(d.ExternalExecTimes) == 0 {
			log.Fatalf("Found 0 samples")
		}
		var benchmarkSum time.Duration
		for _, t := range d.ExternalExecTimes {
			benchmarkSum += t
		}
		var qbSum time.Duration
		for _, t := range d.InternalExecTimes {
			qbSum += t
		}
		var compilationSum time.Duration
		for _, t := range d.CompilationTimes {
			compilationSum += t
		}
		numErrors := 0
		for _, e := range d.Errors {
			if e != nil {
				numErrors++
			}
		}
		numRuns := len(d.InternalExecTimes)
		e := execStats{
			Name:                name,
			NumRuns:             numRuns,
			MeanBenchmarkTime:   benchmarkSum / time.Duration(numRuns),
			MeanQBTime:          qbSum / time.Duration(numRuns),
			MeanCompilationTime: compilationSum / time.Duration(numRuns),
			NumErrors:           numErrors,
		}
		scriptStats = append(scriptStats, e)
	}
	return &scriptStats
}

func main() {
	services.PostFlagSetupAndParse()
	repeatCount := viper.GetInt("num_runs")
	cloudAddr := viper.GetString("cloud_addr")
	bundleFile := viper.GetString("bundle")

	br, err := createBundleReader(bundleFile)
	if err != nil {
		log.WithError(err).Fatal("Failed to read script bundle")
	}

	data := make(map[string]ScriptExecData)
	scripts := br.GetScripts()
	log.Infof("Running %d scripts %d times each", len(scripts), repeatCount)

	v, err := vizier.ConnectToAllViziers(cloudAddr)
	if err != nil {
		log.WithError(err).Fatalf("Failed to connect to '%s'", cloudAddr)
	}

	for i, s := range scripts {
		if isBlacklist(s.ScriptName) {
			continue
		}
		log.WithField("script", s.ScriptName).WithField("idx", i).Infof("Executing new script")

		externalExecTiming := make([]time.Duration, repeatCount)
		internalExecTiming := make([]time.Duration, repeatCount)
		compilationTiming := make([]time.Duration, repeatCount)
		scriptErrors := make([]error, repeatCount)

		// Run script
		for i := 0; i < repeatCount; i++ {
			res, err := exec(v, s)
			if err != nil {
				log.WithError(err).Fatalf("Failed to execute script")
			}
			externalExecTiming[i] = res.externalExecTime
			compilationTiming[i] = res.compileTime
			internalExecTiming[i] = res.internalExecTime
			scriptErrors[i] = res.scriptErr
		}

		data[s.ScriptName] = ScriptExecData{
			ExternalExecTimes: externalExecTiming,
			InternalExecTimes: internalExecTiming,
			CompilationTimes:  compilationTiming,
			Errors:            scriptErrors,
		}
	}

	s := &stdoutTableWriter{}
	err = s.Write(&data)
	if err != nil {
		log.WithError(err).Fatalf("Failure on writing table")
	}
}
