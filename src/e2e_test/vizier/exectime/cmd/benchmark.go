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
	"encoding/json"
	"errors"
	"fmt"
	"math"
	"math/rand"
	"os"
	"sort"
	"strings"
	"time"

	"github.com/gofrs/uuid"
	"github.com/olekukonko/tablewriter"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"

	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils/script"
)

var disallowedScripts = map[string]bool{
	"px/http2_data": true,
}

var allowedOutputFmts = map[string]bool{
	"table": true,
	"json":  true,
}

const defaultBundleFile = "https://artifacts.px.dev/pxl_scripts/bundle.json"

const (
	execTimeExternalLabel = "Exec Time: External"
	execTimeInternalLabel = "Exec Time: Internal"
	compTimeLabel         = "Compilation Time"
	numErrorsLabel        = "Num Errors"
	numBytesLabel         = "Num Bytes"
)

func init() {
	BenchmarkCmd.PersistentFlags().Int("num_runs", 20, "number of times to run a script ")
	BenchmarkCmd.PersistentFlags().StringP("cloud_addr", "a", "withpixie.ai:443", "The address of Pixie Cloud")
	BenchmarkCmd.PersistentFlags().StringP("bundle", "b", defaultBundleFile, "The bundle file to use")
	BenchmarkCmd.PersistentFlags().BoolP("all-clusters", "d", false, "Run script across all clusters")
	BenchmarkCmd.PersistentFlags().BoolP("split-funcs", "p", false, "Run each function from the vis spec separately")
	BenchmarkCmd.PersistentFlags().StringP("cluster", "c", "", "Run only on selected cluster")
	BenchmarkCmd.PersistentFlags().StringSliceP("scripts", "s", nil, "Run only on selected scripts")
	BenchmarkCmd.PersistentFlags().StringP("output", "o", "table", "Output format to use. Currently supports 'table' or 'json'")
	RootCmd.AddCommand(BenchmarkCmd)
}

// Distribution is the interface used to make the stats.
type Distribution interface {
	Summarize() string
	Type() string
	Append(interface{})
	Diff(Distribution) (DistributionDiff, error)
}

// TimeDistribution contains Times and implements the Distribution interface.
type TimeDistribution struct {
	Times []time.Duration
}

// Type returns the type of distribution this is, for json marshalling purposes.
func (t *TimeDistribution) Type() string {
	return "Time"
}

// Append a value to the time distribution.
func (t *TimeDistribution) Append(v interface{}) {
	dur, ok := v.(time.Duration)
	if !ok {
		log.Fatal("failed to append to TimeDistribution")
	}
	t.Times = append(t.Times, dur)
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

// Type returns the type of distribution this is, for json marshalling purposes.
func (d *ErrorDistribution) Type() string {
	return "Error"
}

// Append a value to the error distribution.
func (d *ErrorDistribution) Append(v interface{}) {
	if v == nil {
		d.Errors = append(d.Errors, nil)
		return
	}
	e, ok := v.(error)
	if !ok {
		log.Fatal("failed to append to ErrorDistribution")
	}
	d.Errors = append(d.Errors, e)
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

// Type returns the type of distribution this is, for json marshalling purposes.
func (d *BytesDistribution) Type() string {
	return "Bytes"
}

// Append a value to the bytes distribution.
func (d *BytesDistribution) Append(v interface{}) {
	e, ok := v.(int)
	if !ok {
		log.Fatal("failed to append to BytesDistribution")
	}
	d.Bytes = append(d.Bytes, e)
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
	br, err := script.NewBundleManagerWithOrg([]string{bundleFile}, "", "")
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
	resp, err := vizier.RunScript(ctx, v, execScript, nil)
	if err != nil {
		return nil, err
	}

	// Accumulate the streamed data and block until all data is received.
	tw := vizier.NewStreamOutputAdapter(ctx, resp, vizier.FormatInMemory, nil)
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
		execRes.scriptErr = err
		return &execRes, nil
	}
	execRes.internalExecTime = time.Duration(execStats.Timing.ExecutionTimeNs)
	execRes.compileTime = time.Duration(execStats.Timing.CompilationTimeNs)
	execRes.numBytes = tw.TotalBytes()
	return &execRes, nil
}

func isAllowed(s *script.ExecutableScript, allowedScripts map[string]bool) bool {
	if disallowedScripts[s.ScriptName] {
		return false
	}
	if isMutation(s) {
		return false
	}
	if len(allowedScripts) == 0 {
		return true
	}
	return allowedScripts[s.ScriptName]
}

func isMutation(s *script.ExecutableScript) bool {
	return strings.Contains(s.ScriptString, "pxtrace")
}

type distributionMap map[string]Distribution
type distributionContainer struct {
	Type      string
	TimeDist  *TimeDistribution  `json:",omitempty"`
	BytesDist *BytesDistribution `json:",omitempty"`
	ErrorDist *ErrorDistribution `json:",omitempty"`
}

func (dm *distributionMap) MarshalJSON() ([]byte, error) {
	containers := make(map[string]*distributionContainer, len(*dm))
	for k, dist := range *dm {
		containers[k] = &distributionContainer{
			Type: dist.Type(),
		}
		switch dist.Type() {
		case (&TimeDistribution{}).Type():
			timeDist, _ := dist.(*TimeDistribution)
			containers[k].TimeDist = timeDist
		case (&BytesDistribution{}).Type():
			byteDist, _ := dist.(*BytesDistribution)
			containers[k].BytesDist = byteDist
		case (&ErrorDistribution{}).Type():
			errorDist, _ := dist.(*ErrorDistribution)
			containers[k].ErrorDist = errorDist
		}
	}
	return json.Marshal(containers)
}

func (dm *distributionMap) UnmarshalJSON(data []byte) error {
	var containers map[string]*distributionContainer
	err := json.Unmarshal(data, &containers)
	if err != nil {
		return err
	}
	(*dm) = make(distributionMap, len(containers))
	for k, container := range containers {
		switch container.Type {
		case (&TimeDistribution{}).Type():
			(*dm)[k] = container.TimeDist
		case (&BytesDistribution{}).Type():
			(*dm)[k] = container.BytesDist
		case (&ErrorDistribution{}).Type():
			(*dm)[k] = container.ErrorDist
		}
	}
	return nil
}

// ScriptExecData contains the data for a single executed script.
type ScriptExecData struct {
	// The Name of the script we're running.
	Name string
	// The Distributions of Statistics to record.
	Distributions distributionMap
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

func getArgDefaults(v []*vizier.Connector) (map[string]script.Arg, error) {
	argMap := make(map[string]script.Arg)
	argMap["start_time"] = script.Arg{Name: "start_time", Value: "-5m"}
	// Run a script that gets the busiest pod, service, and namespace + the node of that pod.
	pxl := `
import pxviews
import px
df = pxviews.container_process_summary('-5m', px.now())
df = df[df.pod != '' and df.service != '']
max_cpu = df.agg(max_cpu=('cpu_usage', px.max))
df = df.merge(max_cpu, left_on=['cpu_usage'], right_on=['max_cpu'], how='inner')
px.display(df[['pod', 'service', 'namespace', 'node']])
`

	execScript := &script.ExecutableScript{
		ScriptName:   "get_arg_defaults",
		ScriptString: pxl,
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	// Start running the streaming script.
	resp, err := vizier.RunScript(ctx, v, execScript, nil)
	if err != nil {
		return nil, err
	}

	// Accumulate the streamed data and block until all data is received.
	tw := vizier.NewStreamOutputAdapter(ctx, resp, vizier.FormatInMemory, nil)
	err = tw.Finish()

	if err != nil {
		log.WithError(err).Infof("Error '%s' on '%s'", vizier.FormatErrorMessage(err), execScript.ScriptName)
		return nil, err
	}

	views, err := tw.Views()
	if err != nil {
		log.WithError(err).Error("couldn't get views")
		return nil, err
	}
	for _, table := range views {
		if table.Name() != "output" {
			continue
		}

		argMap["pod"] = script.Arg{Name: "pod", Value: table.Data()[0][0].(string)}
		argMap["service"] = script.Arg{Name: "service", Value: table.Data()[0][1].(string)}
		argMap["namespace"] = script.Arg{Name: "namespace", Value: table.Data()[0][2].(string)}
		argMap["node"] = script.Arg{Name: "node", Value: table.Data()[0][3].(string)}
	}

	if len(argMap) != 5 {
		return nil, errors.New("couldn't get arg defaults from pxl script")
	}
	return argMap, nil
}

func benchmarkCmd(cmd *cobra.Command) {
	// Set the logger to use stderr so that json output can be consumed without log lines.
	log.SetOutput(os.Stderr)

	repeatCount, _ := cmd.Flags().GetInt("num_runs")
	cloudAddr, _ := cmd.Flags().GetString("cloud_addr")
	bundleFile, _ := cmd.Flags().GetString("bundle")
	allClusters, _ := cmd.Flags().GetBool("all-clusters")
	selectedCluster, _ := cmd.Flags().GetString("cluster")
	selectedScripts, _ := cmd.Flags().GetStringSlice("scripts")
	outputFmt, _ := cmd.Flags().GetString("output")
	splitByFunc, _ := cmd.Flags().GetBool("split-funcs")

	clusterID := uuid.FromStringOrNil(selectedCluster)

	if !allowedOutputFmts[outputFmt] {
		log.WithField("output", outputFmt).Fatal("invalid output format")
	}

	br, err := createBundleReader(bundleFile)
	if err != nil {
		log.WithError(err).Fatal("Failed to read script bundle")
	}

	scripts := br.GetScripts()

	if !allClusters && clusterID == uuid.Nil {
		clusterID, err = vizier.FirstHealthyVizier(cloudAddr)
		if err != nil {
			log.WithError(err).Fatal("Could not fetch healthy vizier")
		}
	}

	allowedScripts := make(map[string]bool)
	for _, s := range selectedScripts {
		allowedScripts[s] = true
	}

	vzrConns := vizier.MustConnectHealthyDefaultVizier(cloudAddr, allClusters, clusterID)

	argDefaults, err := getArgDefaults(vzrConns)
	if err != nil {
		log.WithError(err).Fatal("Failed to get arg defaults")
	}

	viableScripts := make([]*script.ExecutableScript, 0)
	for _, s := range scripts {
		if !isAllowed(s, allowedScripts) {
			continue
		}

		s.Args = make(map[string]script.Arg)
		for k, v := range argDefaults {
			s.Args[k] = v
		}

		for _, v := range s.Vis.Variables {
			if _, ok := s.Args[v.Name]; ok {
				continue
			}
			value := ""
			if len(v.ValidValues) > 0 {
				value = v.ValidValues[0]
			}
			if v.DefaultValue != nil {
				value = v.DefaultValue.Value
			}
			s.Args[v.Name] = script.Arg{Name: v.Name, Value: value}
		}
		if !splitByFunc || s.Vis == nil {
			viableScripts = append(viableScripts, s)
			continue
		}

		// Make the script name base + the name of the function
		// Make the only function the global function to go through.

		scriptFuncs := make([]*vispb.Vis_GlobalFunc, 0)
		if s.Vis.GlobalFuncs != nil {
			scriptFuncs = append(scriptFuncs, s.Vis.GlobalFuncs...)
		}
		for _, w := range s.Vis.Widgets {
			switch w.FuncOrRef.(type) {
			case *vispb.Widget_Func_:
				scriptFuncs = append(scriptFuncs, &vispb.Vis_GlobalFunc{
					OutputName: w.GetFunc().Name,
					Func:       w.GetFunc(),
				})
			default:
				// Skip if it's not a function definition.
				continue
			}
		}

		s.Vis.Widgets = nil
		for _, f := range scriptFuncs {
			newScript := &script.ExecutableScript{
				ScriptString: s.ScriptString,
				ScriptName:   s.ScriptName + "/" + f.Func.Name,
				Vis: &vispb.Vis{
					GlobalFuncs: []*vispb.Vis_GlobalFunc{f},
					Variables:   s.Vis.Variables,
				},
				Args: s.Args,
			}
			viableScripts = append(viableScripts, newScript)
		}
	}

	log.Infof("Running %d scripts %d times each", len(viableScripts), repeatCount)
	data := make(map[string]*ScriptExecData)
	scriptsToRun := make([]*script.ExecutableScript, 0)
	for _, s := range viableScripts {
		for i := 0; i < repeatCount; i++ {
			scriptsToRun = append(scriptsToRun, s)
		}
		externalExecTiming := make([]time.Duration, 0)
		internalExecTiming := make([]time.Duration, 0)
		compilationTiming := make([]time.Duration, 0)
		scriptErrors := make([]error, 0)
		numBytes := make([]int, 0)
		data[s.ScriptName] = &ScriptExecData{
			Name: s.ScriptName,
			Distributions: distributionMap{
				execTimeExternalLabel: &TimeDistribution{externalExecTiming},
				execTimeInternalLabel: &TimeDistribution{internalExecTiming},
				compTimeLabel:         &TimeDistribution{compilationTiming},
				numErrorsLabel:        &ErrorDistribution{scriptErrors},
				numBytesLabel:         &BytesDistribution{numBytes},
			},
		}
	}

	// Shuffle scripts to run, to increase independence of samples across time.
	r := rand.New(rand.NewSource(42))
	r.Shuffle(len(scriptsToRun), func(i, j int) {
		scriptsToRun[i], scriptsToRun[j] = scriptsToRun[j], scriptsToRun[i]
	})

	// Run scripts in shuffled order.
	for _, s := range scriptsToRun {
		// Run script.
		log.WithField("script", s.ScriptName).Infof("Executing script")
		res, err := executeScript(vzrConns, s)
		if err != nil {
			log.WithError(err).Fatalf("Failed to execute script")
		}
		dists := data[s.ScriptName].Distributions
		dists[numErrorsLabel].Append(res.scriptErr)
		dists[execTimeExternalLabel].Append(res.externalExecTime)
		dists[compTimeLabel].Append(res.compileTime)
		dists[execTimeInternalLabel].Append(res.internalExecTime)
		dists[numBytesLabel].Append(res.numBytes)
	}

	if outputFmt == "table" {
		s := &stdoutTableWriter{}
		// Sort by key names.
		sortedData := sortByKeys(&data)
		err = s.Write(&sortedData)
		if err != nil {
			log.WithError(err).Fatalf("Failure on writing table")
		}
	}
	if outputFmt == "json" {
		jsonData, err := json.Marshal(data)
		if err != nil {
			log.WithError(err).Fatal("Failed to marshal results to json")
		}
		os.Stdout.Write(jsonData)
	}
}

// RootCmd executes the subcommands.
var RootCmd = &cobra.Command{
	Use:   "exectime_benchmark",
	Short: "Run and compare exectime benchmarks",
}

// BenchmarkCmd executes the script execution benchmark.
var BenchmarkCmd = &cobra.Command{
	Use:   "benchmark",
	Short: "Run exec time benchmarks",
	Run: func(cmd *cobra.Command, args []string) {
		benchmarkCmd(cmd)
	},
}
