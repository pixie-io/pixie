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

package suites

import (
	"fmt"
	"time"

	pb "px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
)

// ExperimentSuite is a group of experiments, represented as a function that returns multiple named experiment specs.
type ExperimentSuite func() map[string]*pb.ExperimentSpec

// ExperimentSuiteRegistry contains all the ExperimentSuite, keyed by name.
var ExperimentSuiteRegistry = map[string]ExperimentSuite{
	"nightly":   nightlyExperimentSuite,
	"http-grid": httpGridSuite,
}

func nightlyExperimentSuite() map[string]*pb.ExperimentSpec {
	defaultMetricPeriod := 30 * time.Second
	preDur := 5 * time.Minute
	dur := 40 * time.Minute
	httpNumConns := 100
	exps := map[string]*pb.ExperimentSpec{
		"http-loadtest/100/100":  HTTPLoadTestExperiment(httpNumConns, 100, defaultMetricPeriod, preDur, dur),
		"http-loadtest/100/3000": HTTPLoadTestExperiment(httpNumConns, 3000, defaultMetricPeriod, preDur, dur),
		"sock-shop":              SockShopExperiment(defaultMetricPeriod, preDur, dur),
	}
	for _, e := range exps {
		addTags(e, "suite/nightly")
	}
	return exps
}

func httpGridSuite() map[string]*pb.ExperimentSpec {
	defaultMetricPeriod := 30 * time.Second
	preDur := 5 * time.Minute
	dur := 40 * time.Minute

	conns := []int{
		10,
		100,
		250,
		500,
	}
	rps := []int{
		100,
		1000,
		2500,
		5000,
	}
	type param struct {
		numConns  int
		targetRPS int
	}
	combos := make([]*param, 0, len(conns)*len(rps))
	for _, numConns := range conns {
		for _, targetRPS := range rps {
			combos = append(combos, &param{
				numConns:  numConns,
				targetRPS: targetRPS,
			})
		}
	}

	exps := make(map[string]*pb.ExperimentSpec)
	for _, p := range combos {
		name := fmt.Sprintf("http-loadtest/%d/%d", p.numConns, p.targetRPS)
		exps[name] = HTTPLoadTestExperiment(p.numConns, p.targetRPS, defaultMetricPeriod, preDur, dur)
	}

	for _, e := range exps {
		addTags(e, "suite/http-grid")
	}
	return exps
}
