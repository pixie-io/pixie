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

package datastudio

import (
	// embed is used to load chart/query templates.
	_ "embed"
	"fmt"
	"os"
	"path"
	"path/filepath"
	"strings"
	"text/template"

	"github.com/Masterminds/sprig/v3"
)

// GenerateViews generates all of the datastudio charts/queries from templates.
func GenerateViews(outPath string, project string, dataset string, reportID string, expPageID string) error {
	queryOutPath := path.Join(outPath, "queries")
	if err := os.MkdirAll(queryOutPath, 0775); err != nil {
		return err
	}
	chartOutPath := path.Join(outPath, "charts")
	if err := os.MkdirAll(chartOutPath, 0775); err != nil {
		return err
	}

	for _, vals := range suiteViews {
		vals.Project = project
		vals.Dataset = dataset
		vals.DSReportID = reportID
		vals.DSExperimentPageID = expPageID
		path := filepath.Join(queryOutPath, fmt.Sprintf("suite_%s.sql", vals.MetricName))
		err := executeTemplate(vals, suiteTemplate, path)
		if err != nil {
			return err
		}

		path = filepath.Join(chartOutPath, fmt.Sprintf("suite_%s.json", vals.MetricName))
		templ := suiteChartTemplates[vals.Unit]
		if err := executeTemplate(vals, templ, path); err != nil {
			return err
		}
	}

	for _, vals := range appOverheadSuiteViews {
		vals.Project = project
		vals.Dataset = dataset
		vals.DSReportID = reportID
		vals.DSExperimentPageID = expPageID
		path := filepath.Join(queryOutPath, fmt.Sprintf("suite_app_overhead_%s.sql", vals.MetricName))
		err := executeTemplate(vals, suiteAppOverheadTemplate, path)
		if err != nil {
			return err
		}

		path = filepath.Join(chartOutPath, fmt.Sprintf("suite_app_overhead_%s.json", vals.MetricName))
		templ := suiteChartTemplates[vals.Unit]
		if err := executeTemplate(vals, templ, path); err != nil {
			return err
		}
	}

	for _, vals := range experimentViews {
		vals.Project = project
		vals.Dataset = dataset
		name := strings.Join(vals.MetricNames, "_")
		path := filepath.Join(queryOutPath, fmt.Sprintf("experiment_%s.sql", name))
		if err := executeTemplate(vals, experimentTemplate, path); err != nil {
			return err
		}

		path = filepath.Join(chartOutPath, fmt.Sprintf("experiment_%s.json", name))
		templ := expChartTemplates[vals.Unit]
		if err := executeTemplate(vals, templ, path); err != nil {
			return err
		}
	}

	path := filepath.Join(queryOutPath, "all_suites_workloads_parameters.sql")
	vals := &struct {
		Project string
		Dataset string
	}{
		Project: project,
		Dataset: dataset,
	}
	if err := executeTemplate(vals, allSuitesWorkloadsParametersTemplate, path); err != nil {
		return err
	}
	return nil
}

//go:embed templates/queries/all_suites_workloads_parameters.sql
var allSuitesWorkloadsParametersTemplate string

//go:embed templates/queries/suite_view.sql
var suiteTemplate string

//go:embed templates/queries/suite_view_app_overhead.sql
var suiteAppOverheadTemplate string

//go:embed templates/queries/experiment_view.sql
var experimentTemplate string

//go:embed templates/charts/suite/bytes.json
var suiteChartByteTemplate string

//go:embed templates/charts/suite/percent.json
var suiteChartPercentTemplate string

//go:embed templates/charts/experiment/percent.json
var expChartPercentTemplate string

//go:embed templates/charts/experiment/bytes.json
var expChartByteTemplate string

//go:embed templates/queries/cpu_usage_preprocessing.sql
var cpuUsagePreprocessing string

var suiteChartTemplates = map[metricUnit]string{
	bytes:   suiteChartByteTemplate,
	percent: suiteChartPercentTemplate,
}

var expChartTemplates = map[metricUnit]string{
	bytes:   expChartByteTemplate,
	percent: expChartPercentTemplate,
}

type metricUnit string

const (
	bytes   metricUnit = "bytes"
	percent metricUnit = "percent"
)

type suiteViewTemplateVals struct {
	ChartTitle          string
	MetricName          string
	MetricsUsed         []string
	MetricSelectExpr    string
	TimeAgg             string
	Unit                metricUnit
	CustomPreprocessing string

	DSReportID         string
	DSExperimentPageID string

	Project string
	Dataset string
}

var suiteViews = []*suiteViewTemplateVals{
	{
		ChartTitle:          "CPU Usage",
		MetricName:          "cpu_usage",
		MetricsUsed:         []string{"cpu_usage", "cpu_seconds_counter"},
		MetricSelectExpr:    "r.cpu_usage",
		TimeAgg:             "",
		Unit:                percent,
		CustomPreprocessing: cpuUsagePreprocessing,
	},
	{
		ChartTitle:       "Max Heap Usage (ignoring table store)",
		MetricName:       "max_memory_ex_table",
		MetricsUsed:      []string{"heap_size_bytes", "table_size"},
		MetricSelectExpr: "r.heap_size_bytes - r.table_size",
		TimeAgg:          "max(max_memory_ex_table)",
		Unit:             bytes,
	},
	{
		ChartTitle:       "Max Heap Usage",
		MetricName:       "max_heap_size",
		MetricsUsed:      []string{"heap_size_bytes"},
		MetricSelectExpr: "r.heap_size_bytes",
		TimeAgg:          "max(max_heap_size)",
		Unit:             bytes,
	},
	{
		ChartTitle:       "Max RSS Memory Usage",
		MetricName:       "max_rss",
		MetricsUsed:      []string{"rss"},
		MetricSelectExpr: "r.rss",
		TimeAgg:          "max(max_rss)",
		Unit:             bytes,
	},
	{
		ChartTitle:       "HTTP Data Loss",
		MetricName:       "http_data_loss",
		MetricsUsed:      []string{"http_data_loss"},
		MetricSelectExpr: "r.http_data_loss",
		TimeAgg:          "array_agg(http_data_loss ORDER BY ts DESC LIMIT 1)[OFFSET(0)]",
		Unit:             percent,
	},
}

var appOverheadSuiteViews = []*suiteViewTemplateVals{
	{
		ChartTitle:          "Application CPU Overhead (% increase over baseline)",
		MetricName:          "cpu_usage",
		MetricsUsed:         []string{"cpu_usage", "cpu_seconds_counter"},
		MetricSelectExpr:    "r.cpu_usage",
		TimeAgg:             "avg(cpu_usage)",
		Unit:                percent,
		CustomPreprocessing: cpuUsagePreprocessing,
	},
	{
		ChartTitle:       "Application RSS Overhead (% increase over baseline)",
		MetricName:       "max_rss",
		MetricsUsed:      []string{"rss"},
		MetricSelectExpr: "r.rss",
		TimeAgg:          "max(max_rss)",
		Unit:             percent,
	},
}

type experimentViewTemplateVals struct {
	ChartTitle          string
	MetricNames         []string
	MetricExprs         []string
	MetricsUsed         []string
	Unit                metricUnit
	CustomPreprocessing string

	Project string
	Dataset string
}

var experimentViews = []*experimentViewTemplateVals{
	{
		ChartTitle:          "CPU Usage",
		MetricNames:         []string{"cpu_usage"},
		MetricExprs:         []string{"r.cpu_usage"},
		MetricsUsed:         []string{"cpu_seconds_counter", "cpu_usage"},
		Unit:                percent,
		CustomPreprocessing: cpuUsagePreprocessing,
	},
	// TODO(james): combine the memory stats into one chart.
	{
		ChartTitle:  "Heap (ignoring table store)",
		MetricNames: []string{"heap_ex_table_store"},
		MetricsUsed: []string{"heap_size_bytes", "table_size"},
		MetricExprs: []string{"r.heap_size_bytes - r.table_size"},
		Unit:        bytes,
	},
	{
		ChartTitle:  "RSS",
		MetricNames: []string{"rss"},
		MetricsUsed: []string{"rss"},
		MetricExprs: []string{"r.rss"},
		Unit:        bytes,
	},
	{
		ChartTitle:  "HTTP Data Loss",
		MetricNames: []string{"http_data_loss"},
		MetricsUsed: []string{"http_data_loss"},
		MetricExprs: []string{"r.http_data_loss"},
		Unit:        percent,
	},
}

func executeTemplate(vals interface{}, templ string, outputPath string) error {
	t, err := template.New("").Funcs(sprig.TxtFuncMap()).Parse(templ)
	if err != nil {
		return err
	}
	f, err := os.Create(outputPath)
	if err != nil {
		return err
	}
	defer f.Close()

	err = t.Execute(f, vals)
	if err != nil {
		return err
	}
	return nil
}
