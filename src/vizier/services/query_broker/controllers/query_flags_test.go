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

package controllers_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/vizier/services/query_broker/controllers"
)

const validQueryWithFlag = `
#px:set analyze=true
#px:set max_output_rows_per_table=9999

t1 = dataframe(table='http_events').range(start='-30s')

t1['service'] = t1.ctx['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t1['failure'] = t1['http_resp_status'] >= 400
t1['range_group'] = pl.subtract(t1['time_'], pl.modulo(t1['time_'], 1000000000))
`

const validQueryWithoutFlag = `
t1 = dataframe(table='http_events').range(start='-30s')

t1['service'] = t1.ctx['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t1['failure'] = t1['http_resp_status'] >= 400
t1['range_group'] = pl.subtract(t1['time_'], pl.modulo(t1['time_'], 1000000000))
`

const invalidFlag1 = `
#px:set analyze=true extra
`

const invalidFlag2 = `
#px:set analyze,true
`

const nonexistentFlag = `
#px:set ABCD=efgh
`

func TestParseQueryFlags_WithFlag(t *testing.T) {
	qf, err := controllers.ParseQueryFlags(validQueryWithFlag)

	require.NoError(t, err)
	assert.NotNil(t, qf)

	val := qf.GetBool("analyze")
	assert.Equal(t, true, val)

	val = qf.GetBool("invalid_key")
	assert.Equal(t, false, val)

	rows := qf.GetInt64("max_output_rows_per_table")
	assert.Equal(t, int64(9999), rows)
}

func TestParseQueryFlags_NoFlag(t *testing.T) {
	qf, err := controllers.ParseQueryFlags(validQueryWithoutFlag)

	require.NoError(t, err)
	assert.NotNil(t, qf)

	val := qf.GetBool("analyze")
	assert.Equal(t, false, val)

	val = qf.GetBool("invalid_key")
	assert.Equal(t, false, val)

	rows := qf.GetInt64("max_output_rows_per_table")
	assert.Equal(t, int64(10000), rows)
}

func TestParseQueryFlags_InvalidFlag(t *testing.T) {
	qf, err := controllers.ParseQueryFlags(invalidFlag1)
	assert.Nil(t, qf)
	assert.NotNil(t, err)

	qf, err = controllers.ParseQueryFlags(invalidFlag2)
	assert.Nil(t, qf)
	assert.NotNil(t, err)

	qf, err = controllers.ParseQueryFlags(nonexistentFlag)
	assert.Nil(t, qf)
	assert.NotNil(t, err)
}

func TestParseQueryFlags_PlanOptions(t *testing.T) {
	qf, err := controllers.ParseQueryFlags(validQueryWithFlag)

	require.NoError(t, err)
	assert.NotNil(t, qf)

	options := qf.GetPlanOptions()
	assert.Equal(t, options.Explain, false)
	assert.Equal(t, options.Analyze, true)
}
