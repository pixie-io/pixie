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
	"os"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/e2e_test/perf_tool/pkg/suites"
)

// GithubMatrixCmd generates a matrix in json format to be used on github actions to run a suite.
var GithubMatrixCmd = &cobra.Command{
	Use:   "github_matrix",
	Short: "Generate a github actions matrix in json format",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlags(cmd.Flags())
	},
	Run: func(cmd *cobra.Command, args []string) {
		githubMatrixCmd(cmd)
	},
}

func init() {
	GithubMatrixCmd.Flags().StringSlice("suite", []string{}, "The suite(s) to generate the matrix for")
	RootCmd.AddCommand(GithubMatrixCmd)
}

type matrix struct {
	Include []*config `json:"include"`
}

type config struct {
	Suite          string `json:"suite"`
	ExperimentName string `json:"experiment_name"`
}

func githubMatrixCmd(*cobra.Command) {
	suiteNames := viper.GetStringSlice("suite")
	if len(suiteNames) == 0 {
		log.Fatal("must specify at least one suite")
	}

	m := &matrix{
		Include: make([]*config, 0),
	}

	for _, suiteName := range suiteNames {
		suiteFunc, ok := suites.ExperimentSuiteRegistry[suiteName]
		if !ok {
			log.Fatalf("no suite found with name '%s'", suiteName)
		}
		for expName := range suiteFunc() {
			c := &config{
				Suite:          suiteName,
				ExperimentName: expName,
			}
			m.Include = append(m.Include, c)
		}
	}

	enc := json.NewEncoder(os.Stdout)
	if err := enc.Encode(m); err != nil {
		log.WithError(err).Fatal("failed to encode json")
	}
}
