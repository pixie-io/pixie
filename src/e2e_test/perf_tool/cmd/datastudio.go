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
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/e2e_test/perf_tool/datastudio"
)

// GenerateDatastudioCmd generates the queries and charts to use in datastudio.
var GenerateDatastudioCmd = &cobra.Command{
	Use:   "generate_datastudio",
	Short: "Generate the sql queries and vega charts to use in datastudio.",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlags(cmd.Flags())
	},
	Run: func(cmd *cobra.Command, args []string) {
		generateDatastudioCmd(cmd)
	},
}

func init() {
	GenerateDatastudioCmd.Flags().StringP("output", "o", "", "Path to put generated queries and charts")
	GenerateDatastudioCmd.Flags().String("bq_project", "", "Gcloud project containing bq dataset to query from.")
	GenerateDatastudioCmd.Flags().String("bq_dataset", "", "BQ dataset to query from.")
	GenerateDatastudioCmd.Flags().String("ds_report_id", "", "The unique ID of the datastudio report, used for self-links in the datastudio charts")
	GenerateDatastudioCmd.Flags().String("ds_experiment_page_id", "", "The unique ID of the datastudio experiment page, used for self-links in the datastudio charts")

	RootCmd.AddCommand(GenerateDatastudioCmd)
}

func generateDatastudioCmd(*cobra.Command) {
	outPath := viper.GetString("output")
	project := viper.GetString("bq_project")
	datasetName := viper.GetString("bq_dataset")
	reportID := viper.GetString("ds_report_id")
	experimentPageID := viper.GetString("ds_experiment_page_id")

	if err := datastudio.GenerateViews(outPath, project, datasetName, reportID, experimentPageID); err != nil {
		log.WithError(err).Fatal("failed to generate views")
	}
}
