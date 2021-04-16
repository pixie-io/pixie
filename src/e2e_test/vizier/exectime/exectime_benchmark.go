package main

import (
	"context"
	"errors"
	"fmt"
	"math"
	"os"
	"sort"
	"time"

	"github.com/gofrs/uuid"
	"github.com/olekukonko/tablewriter"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/script"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/shared/services"
)

var scriptDisableList = []string{
	"px/http2_data",
}

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle.json"

func init() {
	pflag.Int("num_runs", 10, "number of times to run a script ")
	pflag.StringP("cloud_addr", "a", "withpixie.ai:443", "The address of Pixie Cloud")
	pflag.StringP("bundle", "b", defaultBundleFile, "The bundle file to use")
	pflag.BoolP("all-clusters", "d", false, "Run script across all clusters")
	pflag.StringP("cluster", "c", "", "Run only on selected cluster")
}

// Distribution is the interface used to make the stats.
type Distribution interface {
	Summarize() string
}

// TimeDistribution contains Times and implements the Distribution interface.
type TimeDistribution struct {
	Times []time.Duration
}

// Mean caluates the mean of the time distribution.
func (t *TimeDistribution) Mean() time.Duration {
	var sum time.Duration
	for _, t := range t.Times {
		sum += t
	}
	return sum / time.Duration(len(t.Times))
}

// Stddev calculates the stddev of the time distribution.
func (t *TimeDistribution) Stddev() time.Duration {
	var sumOfSquares float64
	mean := t.Mean()
	for _, t := range t.Times {
		sumOfSquares += math.Pow(float64(t-mean), 2)
	}
	return time.Duration(math.Sqrt(sumOfSquares / float64(len(t.Times))))
}

// Summarize returns the Mean +/- stddev.
func (t *TimeDistribution) Summarize() string {
	return fmt.Sprintf("%v +/- %v", t.Mean().Round(time.Duration(10)*time.Microsecond), t.Stddev().Round(time.Duration(10)*time.Microsecond))
}

// ErrorDistribution contains Errors.
type ErrorDistribution struct {
	Errors []error
}

// Num counts the number of errors.
func (d *ErrorDistribution) Num() int {
	var numErrs int
	for _, e := range d.Errors {
		if e != nil {
			numErrs++
		}
	}
	return numErrs
}

// Summarize returns the number of errors.
func (d *ErrorDistribution) Summarize() string {
	return fmt.Sprintf("%d", d.Num())
}

// BytesDistribution contains Bytess and implements the Distribution interface.
type BytesDistribution struct {
	Bytes []int
}

// Mean caluates the mean of the time distribution.
func (d *BytesDistribution) Mean() float64 {
	var sum int
	for _, b := range d.Bytes {
		sum += b
	}
	return float64(sum) / float64(len(d.Bytes))
}

// Summarize returns the Mean +/- stddev.
func (d *BytesDistribution) Summarize() string {
	return fmt.Sprintf("%.2f +/- %.2f", d.Mean(), d.Stddev())
}

// Stddev calculates the stddev of the time distribution.
func (d *BytesDistribution) Stddev() float64 {
	var sumOfSquares float64
	mean := d.Mean()
	for _, b := range d.Bytes {
		sumOfSquares += math.Pow(float64(b)-mean, 2)
	}
	return math.Sqrt(sumOfSquares / float64(len(d.Bytes)))
}

func createBundleReader(bundleFile string) (*script.BundleManager, error) {
	br, err := script.NewBundleManager([]string{bundleFile})
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
	numBytes         int
}

func executeScript(v []*vizier.Connector, execScript *script.ExecutableScript) (*execResults, error) {
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
	tw := vizier.NewStreamOutputAdapter(ctx, resp, vizier.FormatInMemory)
	err = tw.Finish()

	// Calculate the execution time.
	execRes.externalExecTime = time.Since(start)
	if err != nil {
		log.WithError(err).Infof("Error '%s' on '%s'", vizier.FormatErrorMessage(err), execScript.ScriptName)
		// Store any error that comes up during execution.
		execRes.scriptErr = err
		return &execRes, nil
	}

	// Get the exec stats collected during the stream accumulation.
	execStats, err := tw.ExecStats()
	if err != nil {
		return nil, err
	}
	execRes.internalExecTime = time.Duration(execStats.Timing.ExecutionTimeNs)
	execRes.compileTime = time.Duration(execStats.Timing.CompilationTimeNs)
	execRes.numBytes = tw.TotalBytes()
	return &execRes, nil
}

func isDisabled(script string) bool {
	for _, t := range scriptDisableList {
		if script == t {
			return true
		}
	}
	return false
}

// ScriptExecData contains the data for a single executed script.
type ScriptExecData struct {
	// The Name of the script we're running.
	Name string
	// The Distributions of Statistics to record.
	Distributions map[string]Distribution
}

// stdoutTableWriter writes the execStats out to a table in stdout. Implements ExecStatsWriter.
type stdoutTableWriter struct {
}

func sortByKeys(data *map[string]*ScriptExecData) []*ScriptExecData {
	sorted := make([]string, 0)
	for name := range *data {
		sorted = append(sorted, name)
	}

	sort.Strings(sorted)

	scriptStats := make([]*ScriptExecData, len(*data))
	for i, name := range sorted {
		scriptStats[i] = (*data)[name]
	}
	return scriptStats
}

func (s *stdoutTableWriter) Write(data *[]*ScriptExecData) error {
	if len(*data) == 0 {
		return errors.New("Data has no elements")
	}

	// Setup keys to use across all distributions.
	keys := make([]string, 0)
	for k := range (*data)[0].Distributions {
		keys = append(keys, k)
	}
	sort.Strings(keys)

	table := tablewriter.NewWriter(os.Stdout)
	table.SetHeader(append([]string{"Name"}, keys...))

	// Iterate through data and create table rows.
	for _, d := range *data {
		row := []string{
			d.Name,
		}
		for _, k := range keys {
			val, ok := d.Distributions[k]
			if !ok {
				return fmt.Errorf("Missing key '%s' for script '%s'", k, d.Name)
			}
			row = append(row, val.Summarize())
		}
		table.Append(row)
	}
	table.Render()
	return nil
}

func main() {
	services.PostFlagSetupAndParse()
	repeatCount := viper.GetInt("num_runs")
	cloudAddr := viper.GetString("cloud_addr")
	bundleFile := viper.GetString("bundle")
	allClusters := viper.GetBool("all-clusters")
	selectedCluster := viper.GetString("cluster")
	clusterID := uuid.FromStringOrNil(selectedCluster)

	br, err := createBundleReader(bundleFile)
	if err != nil {
		log.WithError(err).Fatal("Failed to read script bundle")
	}

	scripts := br.GetScripts()
	log.Infof("Running %d scripts %d times each", len(scripts), repeatCount)

	if !allClusters && clusterID == uuid.Nil {
		clusterID, err = vizier.FirstHealthyVizier(cloudAddr)
		if err != nil {
			log.WithError(err).Fatal("Could not fetch healthy vizier")
		}
	}

	vzrConns := vizier.MustConnectDefaultVizier(cloudAddr, allClusters, clusterID)

	data := make(map[string]*ScriptExecData)
	for i, s := range scripts {
		if isDisabled(s.ScriptName) {
			continue
		}
		log.WithField("script", s.ScriptName).WithField("idx", i).Infof("Executing new script")

		externalExecTiming := make([]time.Duration, repeatCount)
		internalExecTiming := make([]time.Duration, repeatCount)
		compilationTiming := make([]time.Duration, repeatCount)
		scriptErrors := make([]error, repeatCount)
		numBytes := make([]int, repeatCount)

		// Run script.
		for i := 0; i < repeatCount; i++ {
			res, err := executeScript(vzrConns, s)
			if err != nil {
				log.WithError(err).Fatalf("Failed to execute script")
			}
			scriptErrors[i] = res.scriptErr
			externalExecTiming[i] = res.externalExecTime
			compilationTiming[i] = res.compileTime
			internalExecTiming[i] = res.internalExecTime
			numBytes[i] = res.numBytes
		}

		data[s.ScriptName] = &ScriptExecData{
			Name: s.ScriptName,
			Distributions: map[string]Distribution{
				"Exec Time: External": &TimeDistribution{externalExecTiming},
				"Exec Time: Internal": &TimeDistribution{internalExecTiming},
				"Compilation Time":    &TimeDistribution{compilationTiming},
				"Num Errors":          &ErrorDistribution{scriptErrors},
				"Num Bytes":           &BytesDistribution{numBytes},
			},
		}
	}

	s := &stdoutTableWriter{}
	// Sort by key names.
	sortedData := sortByKeys(&data)
	err = s.Write(&sortedData)
	if err != nil {
		log.WithError(err).Fatalf("Failure on writing table")
	}
}
