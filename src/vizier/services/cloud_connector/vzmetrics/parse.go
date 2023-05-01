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

package vzmetrics

import (
	"io"
	"sort"
	"strings"

	"github.com/prometheus/common/expfmt"
	"github.com/prometheus/common/model"
	"github.com/prometheus/prometheus/prompb"

	version "px.dev/pixie/src/shared/goversion"
)

// ParsePrometheusTextToWriteReq parses prometheus metrics in prometheus' text-based exposition format, into a prometheus WriteRequest protobuf.
func ParsePrometheusTextToWriteReq(text string, clusterID string, podName string) (*prompb.WriteRequest, error) {
	dec := &expfmt.SampleDecoder{
		Dec: expfmt.NewDecoder(strings.NewReader(text), expfmt.FmtText),
		Opts: &expfmt.DecodeOptions{
			// Default timestamp for metrics that come in without a timestamp.
			Timestamp: model.Now(),
		},
	}
	var samples model.Vector
	for {
		var s model.Vector
		err := dec.Decode(&s)
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, err
		}
		samples = append(samples, s...)
	}
	for _, s := range samples {
		s.Metric["cluster_id"] = model.LabelValue(clusterID)
		s.Metric["pod_name"] = model.LabelValue(podName)
		s.Metric["vizier_version"] = model.LabelValue(version.GetVersion().ToString())
	}
	return toWriteRequest(samples), nil
}

// The remaining code in this file is from Prometheus, and its license is reproduced below:

// Copyright 2017 The Prometheus Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

func toWriteRequest(samples []*model.Sample) *prompb.WriteRequest {
	req := &prompb.WriteRequest{
		Timeseries: make([]prompb.TimeSeries, 0, len(samples)),
	}

	for _, s := range samples {
		ts := prompb.TimeSeries{
			Labels: metricToLabelProtos(s.Metric),
			Samples: []prompb.Sample{
				{
					Value:     float64(s.Value),
					Timestamp: int64(s.Timestamp),
				},
			},
		}
		req.Timeseries = append(req.Timeseries, ts)
	}

	return req
}

func metricToLabelProtos(metric model.Metric) []prompb.Label {
	labels := make([]prompb.Label, 0, len(metric))
	for k, v := range metric {
		labels = append(labels, prompb.Label{
			Name:  string(k),
			Value: string(v),
		})
	}
	sort.Slice(labels, func(i int, j int) bool {
		return labels[i].Name < labels[j].Name
	})
	return labels
}
