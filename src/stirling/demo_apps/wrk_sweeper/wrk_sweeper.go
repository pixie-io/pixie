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

package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path"
	"regexp"
	"strconv"
	"strings"
)

// Flag strings.
var flagSpecDuration string
var flagSpecThreads string
var flagSpecConnections string
var flagSpecMessageSize string
var flagSpecKIters string
var flagWrkHost string
var flagPrefix string
var flagOutputDir string

func init() {
	flag.StringVar(&flagSpecDuration, "d", "10", "The duration spec")
	flag.StringVar(&flagSpecThreads, "t", "8", "The thread spec")
	flag.StringVar(&flagSpecConnections, "conn", "256", "The connection spec")
	flag.StringVar(&flagSpecMessageSize, "msg", "[128, 256, 512, 1024, 2048]", "The msg size spec")
	flag.StringVar(&flagSpecKIters, "kiters", "[0, 25, 50, 75, 100, 150, 200, 500, 1000]", "The msg size spec")
	flag.StringVar(&flagWrkHost, "host", "localhost:9090", "The hostname:port of the http server")
	flag.StringVar(&flagPrefix, "prefix", "", "The prefix (if any) to add to the file")
	flag.StringVar(&flagOutputDir, "outdir", "", "The directory where results should be written")
}

// SequenceGenerator is an interface to generate a list of int64 numbers.
type SequenceGenerator interface {
	// Generate just greedily generate the numbers for now.
	Generate() []int64
}

// expSequenceGenerator generates an exponetial sequence.
type expSequenceGenerator struct {
	Min  int64
	Max  int64
	Base int64
}

func (g *expSequenceGenerator) Generate() []int64 {
	var lst []int64
	for i := g.Min; i <= g.Max; i *= g.Base {
		lst = append(lst, i)
	}
	return lst
}

// linSequenceGenerator generates a linear sequence.
type linSequenceGenerator struct {
	Min  int64
	Max  int64
	Step int64
}

func (g *linSequenceGenerator) Generate() []int64 {
	var lst []int64
	for i := g.Min; i <= g.Max; i += g.Step {
		lst = append(lst, i)
	}
	return lst
}

// constGenerator produces a single constant value.
type constGenerator struct {
	Val int64
}

func (g *constGenerator) Generate() []int64 {
	return []int64{g.Val}
}

// listSequenceGenerator generates a sequence from a fixed list.
type listSequenceGenerator struct {
	Lst []int64
}

func (g *listSequenceGenerator) Generate() []int64 {
	return g.Lst
}

func parseOrDie(s string) int64 {
	val, err := strconv.ParseInt(s, 10, 64)
	if err != nil {
		panic(err)
	}
	return val
}
func parseSpec(spec string) SequenceGenerator {
	expRegex := regexp.MustCompile(`exp\((\d+), \s*(\d+),\s*(\d+)\)`)
	linRegex := regexp.MustCompile(`lin\((\d+), \s*(\d+),\s*(\d+)\)`)
	expMatches := expRegex.FindStringSubmatch(spec)
	if len(expMatches) == 4 {
		min := parseOrDie(expMatches[1])
		max := parseOrDie(expMatches[2])
		base := parseOrDie(expMatches[3])
		return &expSequenceGenerator{
			Min:  min,
			Max:  max,
			Base: base,
		}
	}

	linMatches := linRegex.FindStringSubmatch(spec)
	if len(linMatches) == 4 {
		min := parseOrDie(linMatches[1])
		max := parseOrDie(linMatches[2])
		step := parseOrDie(linMatches[3])
		return &linSequenceGenerator{
			Min:  min,
			Max:  max,
			Step: step,
		}
	}

	// Try to parse as array
	lstVals := []int64{}
	err := json.Unmarshal([]byte(spec), &lstVals)
	if err == nil {
		// parsing worked, must be a list.
		return &listSequenceGenerator{Lst: lstVals}
	}
	// Try to parse as regular int.
	intval, err := strconv.ParseInt(spec, 10, 64)
	if err != nil {
		panic(fmt.Sprintf("failed to parse spec: %s", spec))
	}
	return &constGenerator{Val: intval}
}

func runWrkWithParams(duration, threads, connections, messageSize, thousandIters int64) {
	specStr := fmt.Sprintf("d%d_t%d_c%d_m%d_i%d", duration, threads, connections, messageSize, thousandIters)
	hostStr := fmt.Sprintf("http://%s/bm?response_size=%d&miters=%.3f", flagWrkHost, messageSize, float64(thousandIters)/1000.0)
	wrkCommandArgs := fmt.Sprintf("--script src/stirling/demo_apps/wrk_sweeper/report.lua -d %d -t %d -c %d %s",
		duration, threads, connections, hostStr)
	argsArr := strings.Split(wrkCommandArgs, " ")
	fmt.Printf("specStr=%s, args=%s\n", specStr, wrkCommandArgs)
	cmd := exec.Command("wrk", argsArr...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	err := cmd.Run()
	if err != nil {
		panic(err.Error())
	}

	var outputFileName string
	if len(flagPrefix) == 0 {
		outputFileName = fmt.Sprintf("%s.csv", specStr)
	} else {
		outputFileName = fmt.Sprintf("%s_%s.csv", flagPrefix, specStr)
	}

	outputFile := path.Join(flagOutputDir, outputFileName)
	// Move the CSV file.
	err = os.Rename("result.csv", outputFile)
	if err != nil {
		panic(err.Error())
	}
}

func main() {
	flag.Parse()

	if flagOutputDir != "" {
		err := os.MkdirAll(flagOutputDir, 0755)
		if err != nil {
			panic("failed to create output dir" + err.Error())
		}
	}

	durationVals := parseSpec(flagSpecDuration).Generate()
	threadVals := parseSpec(flagSpecThreads).Generate()
	connectionVals := parseSpec(flagSpecConnections).Generate()
	messageSizeVals := parseSpec(flagSpecMessageSize).Generate()
	thousandItersVals := parseSpec(flagSpecKIters).Generate()

	totalIters := len(durationVals) * len(threadVals) * len(connectionVals) * len(messageSizeVals) * len(thousandItersVals)
	i := 0
	for _, durationVal := range durationVals {
		for _, threadVal := range threadVals {
			for _, connectionVal := range connectionVals {
				for _, messageSizeVal := range messageSizeVals {
					for _, thousandItersVal := range thousandItersVals {
						i++
						fmt.Printf("Progress: %.2f%%\n", 100*float64(i)/float64(totalIters))
						runWrkWithParams(durationVal, threadVal, connectionVal, messageSizeVal, thousandItersVal)
					}
				}
			}
		}
	}
}
