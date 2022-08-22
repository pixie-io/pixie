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
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"sort"
	"time"

	"github.com/fatih/color"
	"github.com/olekukonko/tablewriter"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
)

func init() {
	CompareCmd.PersistentFlags().StringP("baseline", "b", "", "The json file with the baseline results")
	CompareCmd.PersistentFlags().StringP(
		"change",
		"c",
		"",
		"The json file with the results from the new changes to compare against the baseline")
	CompareCmd.PersistentFlags().StringSlice(
		"columns",
		[]string{execTimeInternalLabel, numBytesLabel},
		"Comma separated list of distributions to show in the table output")
	RootCmd.AddCommand(CompareCmd)
}

// DistributionDiff represents the difference between two distributions.
type DistributionDiff interface {
	Summarize() string
}

// Diff computes the difference between this distribution and another time distribution.
func (t *TimeDistribution) Diff(other Distribution) (DistributionDiff, error) {
	otherTimeDist, ok := other.(*TimeDistribution)
	if !ok {
		return nil, errors.New("TimeDistribution.Diff must be called with another TimeDistribution as argument")
	}
	return &timeDistributionDiff{t, otherTimeDist}, nil
}

// Diff computes the difference between this distribution and another bytes distribution.
func (t *BytesDistribution) Diff(other Distribution) (DistributionDiff, error) {
	otherBytesDist, ok := other.(*BytesDistribution)
	if !ok {
		return nil, errors.New("BytesDistribution.Diff must be called with another BytesDistribution as argument")
	}
	return &bytesDistributionDiff{t, otherBytesDist}, nil
}

// Diff computes the difference between this distribution and another error distribution.
func (t *ErrorDistribution) Diff(other Distribution) (DistributionDiff, error) {
	otherErrorDist, ok := other.(*ErrorDistribution)
	if !ok {
		return nil, errors.New("ErrorDistribution.Diff must be called with another ErrorDistribution as argument")
	}
	return &errorDistributionDiff{t, otherErrorDist}, nil
}

type timeDistributionDiff struct {
	A *TimeDistribution
	B *TimeDistribution
}

func toFloat64Arr(times []time.Duration) []float64 {
	floats := make([]float64, len(times))
	for i, t := range times {
		floats[i] = float64(t) / float64(time.Microsecond)
	}
	return floats
}

// Use a significance level of 0.01
const timeDiffAlpha = 0.01

// Summarize returns a string summary of the difference between the two distributions.
func (d *timeDistributionDiff) Summarize() string {
	meanDiff := d.A.Mean() - d.B.Mean()
	pValue, err := UTest(toFloat64Arr(d.A.Times), toFloat64Arr(d.B.Times))
	ignorePValue := false
	if err != nil {
		log.WithError(err).Error("failed to calculate UTest ignoring p value")
		ignorePValue = true
	}
	pValueStr := fmt.Sprintf("%.2f", pValue)
	if ignorePValue {
		pValueStr = "N/A"
	}
	summary := fmt.Sprintf("%v (%v vs %v, u: %s)",
		meanDiff.Round(10*time.Microsecond),
		d.A.Mean().Round(100*time.Microsecond),
		d.B.Mean().Round(100*time.Microsecond),
		pValueStr)
	if ignorePValue || pValue >= timeDiffAlpha {
		// Null hypothesis cannot be rejected (or there were too few samples to do a U test).
		return summary
	}
	if meanDiff > 0 {
		return color.GreenString(summary)
	}
	return color.RedString(summary)
}

type bytesDistributionDiff struct {
	A *BytesDistribution
	B *BytesDistribution
}

const bytesDiffRedPercentThreshold = 0.05

// Summarize returns a string summary of the difference between the two distributions.
func (d *bytesDistributionDiff) Summarize() string {
	meanDiff := d.A.Mean() - d.B.Mean()
	summary := fmt.Sprintf("%.1fkB (%.1fkB vs %.1fkB)", meanDiff/1024, d.A.Mean()/1024, d.B.Mean()/1024)
	percentDiff := meanDiff / d.A.Mean()
	if percentDiff > bytesDiffRedPercentThreshold || percentDiff < -bytesDiffRedPercentThreshold {
		return color.RedString(summary)
	}
	return summary
}

type errorDistributionDiff struct {
	A *ErrorDistribution
	B *ErrorDistribution
}

// Summarize returns a string summary of the difference between the two distributions.
func (d *errorDistributionDiff) Summarize() string {
	numDiff := d.A.Num() - d.B.Num()
	summary := fmt.Sprintf("%+d", numDiff)
	if numDiff != 0 {
		return color.RedString(summary)
	}
	return color.GreenString(summary)
}

type scriptExecDiff struct {
	Name  string
	Diffs map[string]DistributionDiff
}

// diffTableWriter writes script diffs to a table for comparison.
type diffTableWriter struct {
	Columns []string
}

// Write writes the diffs to a textual table.
func (w *diffTableWriter) Write(data []*scriptExecDiff) error {
	if len(data) == 0 {
		return errors.New("Data has no elements")
	}

	keys := w.Columns

	table := tablewriter.NewWriter(os.Stdout)
	table.SetAutoWrapText(false)
	table.SetHeader(append([]string{"Name"}, keys...))

	// Iterate through data and create table rows.
	for _, d := range data {
		row := []string{
			d.Name,
		}
		for _, k := range keys {
			val, ok := d.Diffs[k]
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

func compareCmd(cmd *cobra.Command) {
	baselineJSONPath, _ := cmd.Flags().GetString("baseline")
	changeJSONPath, _ := cmd.Flags().GetString("change")
	columnsToShow, _ := cmd.Flags().GetStringSlice("columns")

	baselineContent, err := os.ReadFile(baselineJSONPath)
	if err != nil {
		log.WithError(err).Fatal("Failed to read baseline json file")
	}
	changeContent, err := os.ReadFile(changeJSONPath)
	if err != nil {
		log.WithError(err).Fatal("Failed to read change json file")
	}

	var baselineData map[string]*ScriptExecData
	err = json.Unmarshal(baselineContent, &baselineData)
	if err != nil {
		log.WithError(err).Fatal("Failed to unmarshal baseline json data")
	}

	var changeData map[string]*ScriptExecData
	err = json.Unmarshal(changeContent, &changeData)
	if err != nil {
		log.WithError(err).Fatal("Failed to unmarshal change json data")
	}

	diffs := make(map[string]*scriptExecDiff, len(baselineData))
	for k := range baselineData {
		baseExecData := baselineData[k]
		changeExecData, ok := changeData[k]
		if !ok {
			continue
		}

		diffs[k] = &scriptExecDiff{
			Name:  baseExecData.Name,
			Diffs: make(map[string]DistributionDiff),
		}
		for distName := range baseExecData.Distributions {
			baseDist := baseExecData.Distributions[distName]
			changeDist, ok := changeExecData.Distributions[distName]
			if !ok {
				continue
			}
			diff, err := baseDist.Diff(changeDist)
			if err != nil {
				log.WithError(err).Fatal("Failed to diff two distributions")
			}
			diffs[k].Diffs[distName] = diff
		}
	}

	log.Info("All values are `baseline - change`")

	w := &diffTableWriter{columnsToShow}
	sortedNames := make([]string, 0)
	for name := range diffs {
		sortedNames = append(sortedNames, name)
	}
	sort.Strings(sortedNames)

	sortedDiffs := make([]*scriptExecDiff, len(diffs))
	for i, name := range sortedNames {
		sortedDiffs[i] = diffs[name]
	}
	err = w.Write(sortedDiffs)
	if err != nil {
		log.WithError(err).Fatal("failed to write diffs to table")
	}
}

// CompareCmd compares two benchmark results produced by the BenchmarkCmd.
var CompareCmd = &cobra.Command{
	Use:   "compare",
	Short: "Compare the results of two benchmark runs",
	Run: func(cmd *cobra.Command, args []string) {
		compareCmd(cmd)
	},
}
