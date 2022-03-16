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

package vizier_test

import (
	"encoding/json"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
)

func TestBasic(t *testing.T) {
	relation := &vizierpb.Relation{
		Columns: []*vizierpb.Relation_ColumnInfo{
			{
				ColumnName:         "bool",
				ColumnType:         vizierpb.BOOLEAN,
				ColumnSemanticType: vizierpb.ST_NONE,
			},
			{
				ColumnName:         "int",
				ColumnType:         vizierpb.INT64,
				ColumnSemanticType: vizierpb.ST_NONE,
			},
			{
				ColumnName:         "float",
				ColumnType:         vizierpb.FLOAT64,
				ColumnSemanticType: vizierpb.ST_NONE,
			},
			{
				ColumnName:         "string",
				ColumnType:         vizierpb.STRING,
				ColumnSemanticType: vizierpb.ST_NONE,
			},
			{
				ColumnName:         "time",
				ColumnType:         vizierpb.TIME64NS,
				ColumnSemanticType: vizierpb.ST_NONE,
			},
		},
	}

	formatter := vizier.NewDataFormatterForTable(relation)

	// bool
	assert.Equal(t, "true", formatter.FormatValue(0, true))
	assert.Equal(t, "false", formatter.FormatValue(0, false))

	// int
	assert.Equal(t, "-12", formatter.FormatValue(1, -12))
	assert.Equal(t, "1204", formatter.FormatValue(1, 1204))

	// float
	assert.Equal(t, "-0.1", formatter.FormatValue(2, -0.1))
	assert.Equal(t, "121214", formatter.FormatValue(2, 121214.04234))

	// string
	assert.Equal(t, "abc", formatter.FormatValue(3, "abc"))

	// time
	nowTime := time.Unix(0, 1601694759495000000)
	nowTimeStr := nowTime.String()
	assert.Equal(t, nowTimeStr, formatter.FormatValue(4, nowTime))
}

func TestDuration(t *testing.T) {
	relation := &vizierpb.Relation{
		Columns: []*vizierpb.Relation_ColumnInfo{
			{
				ColumnName:         "bool",
				ColumnType:         vizierpb.BOOLEAN,
				ColumnSemanticType: vizierpb.ST_NONE,
			},
			{
				ColumnName:         "duration",
				ColumnType:         vizierpb.INT64,
				ColumnSemanticType: vizierpb.ST_DURATION_NS,
			},
		},
	}

	formatter := vizier.NewDataFormatterForTable(relation)

	assert.Equal(t, "144 ns", formatter.FormatValue(1, int64(144)))
	assert.Equal(t, "5.1 µs", formatter.FormatValue(1, int64(5144)))
	assert.Equal(t, "5.0 ms", formatter.FormatValue(1, int64(5*1000*1000)))
	assert.Equal(t, "13.0 s", formatter.FormatValue(1, int64(13*1000*1000*1000+1242)))
	assert.Equal(t, "5 min 0 s", formatter.FormatValue(1, int64(5*60*1000*1000*1000+1334)))
	assert.Equal(t, "12 hours 0 min", formatter.FormatValue(1, int64(12*60*60*1000*1000*1000+1335144)))
	assert.Equal(t, "25 days 0 hours", formatter.FormatValue(1, int64(25*24*60*60*1000*1000*1000+133514124)))
}

func TestScriptReferences(t *testing.T) {
	relation := &vizierpb.Relation{
		Columns: []*vizierpb.Relation_ColumnInfo{
			{
				ColumnName:         "source",
				ColumnType:         vizierpb.STRING,
				ColumnSemanticType: vizierpb.ST_SCRIPT_REFERENCE,
			},
		},
	}

	formatter := vizier.NewDataFormatterForTable(relation)

	assert.Equal(t, "px-sock-shop/load-test-799f9dffff-s6m2c", formatter.FormatValue(0, `{"label":"px-sock-shop/load-test-799f9dffff-s6m2c","script":"px/pod","args":{"start_time":"-5m","pod":"px-sock-shop/load-test-799f9dffff-s6m2c"}}`))
}
func TestBytes(t *testing.T) {
	relation := &vizierpb.Relation{
		Columns: []*vizierpb.Relation_ColumnInfo{
			{
				ColumnName:         "bytes",
				ColumnType:         vizierpb.INT64,
				ColumnSemanticType: vizierpb.ST_BYTES,
			},
		},
	}

	formatter := vizier.NewDataFormatterForTable(relation)

	assert.Equal(t, "144 B", formatter.FormatValue(0, int64(144)))
	assert.Equal(t, "5.0 KiB", formatter.FormatValue(0, int64(5144)))
	assert.Equal(t, "4.8 MiB", formatter.FormatValue(0, int64(5*1000*1000)))
	assert.Equal(t, "12.1 GiB", formatter.FormatValue(0, int64(13*1000*1000*1000+1242)))
	assert.Equal(t, "39.3 TiB", formatter.FormatValue(0, int64(12*60*60*1000*1000*1000+1335144)))
}

func TestThroughput(t *testing.T) {
	relation := &vizierpb.Relation{
		Columns: []*vizierpb.Relation_ColumnInfo{
			{
				ColumnName:         "throughput",
				ColumnType:         vizierpb.FLOAT64,
				ColumnSemanticType: vizierpb.ST_THROUGHPUT_PER_NS,
			},
			{
				ColumnName:         "throughput_bytes",
				ColumnType:         vizierpb.FLOAT64,
				ColumnSemanticType: vizierpb.ST_THROUGHPUT_BYTES_PER_NS,
			},
		},
	}

	formatter := vizier.NewDataFormatterForTable(relation)

	assert.Equal(t, "-0.002342 /sec", formatter.FormatValue(0, float64(-0.0000000000023423424)))
	assert.Equal(t, "1243924924.000000 /sec", formatter.FormatValue(0, float64(1.243924924)))

	assert.Equal(t, "-229 KiB/sec", formatter.FormatValue(1, float64(-0.00023423424)))
	assert.Equal(t, "1.2 GiB/sec", formatter.FormatValue(1, float64(1.243924924)))
}

func TestPercent(t *testing.T) {
	relation := &vizierpb.Relation{
		Columns: []*vizierpb.Relation_ColumnInfo{
			{
				ColumnName:         "percent",
				ColumnType:         vizierpb.FLOAT64,
				ColumnSemanticType: vizierpb.ST_PERCENT,
			},
		},
	}

	formatter := vizier.NewDataFormatterForTable(relation)

	assert.Equal(t, "-23.44 %", formatter.FormatValue(0, float64(-0.2344)))
	assert.Equal(t, "1223.40 %", formatter.FormatValue(0, float64(12.234)))
	assert.Equal(t, "14.00 %", formatter.FormatValue(0, float64(0.140000)))
}

func TestQuantiles(t *testing.T) {
	relation := &vizierpb.Relation{
		Columns: []*vizierpb.Relation_ColumnInfo{
			{
				ColumnName:         "quantiles",
				ColumnType:         vizierpb.STRING,
				ColumnSemanticType: vizierpb.ST_QUANTILES,
			},
			{
				ColumnName:         "quantiles_duration_ns",
				ColumnType:         vizierpb.STRING,
				ColumnSemanticType: vizierpb.ST_DURATION_NS_QUANTILES,
			},
		},
	}

	formatter := vizier.NewDataFormatterForTable(relation)

	rate1, err := json.Marshal(map[string]interface{}{
		"p50": float64(-1231),
		"p99": float64(0.000023423),
	})
	require.NoError(t, err)
	rate2, err := json.Marshal(map[string]interface{}{
		"p50": float64(9234234.3),
		"p99": float64(42398243.001),
	})
	require.NoError(t, err)

	assert.Equal(t, "p50: -1231, p99: 2.3423e-05", formatter.FormatValue(0, string(rate1)))
	assert.Equal(t, "p50: 9.23423e+06, p99: 4.23982e+07", formatter.FormatValue(0, string(rate2)))

	duration1, err := json.Marshal(map[string]interface{}{
		"p50": float64(-12313),
		"p99": float64(0.0001),
	})
	require.NoError(t, err)
	duration2, err := json.Marshal(map[string]interface{}{
		"p50": float64(23409234),
		"p99": float64(234092234234),
	})
	require.NoError(t, err)

	assert.Equal(t, "p50: -12.3 µs, p99: 0 s", formatter.FormatValue(1, string(duration1)))
	assert.Equal(t, "p50: 23.4 ms, p99: 3 min 54 s", formatter.FormatValue(1, string(duration2)))
}
