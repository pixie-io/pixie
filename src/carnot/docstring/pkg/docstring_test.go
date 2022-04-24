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

package docstring

import (
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/carnot/docspb"
	"px.dev/pixie/src/carnot/udfspb"
)

// Valid doc strings
const dataFrameDoc = `DataFrame represents processed data in Pxl

DataFrame is the primary data structure in Pxl. It exposes data transformation operations through its API.
Examples:
	# Create a dataframe object
	df = px.DataFrame('http_events', start_time='-5m')
	# Aggregate to calculate number of events in the http_events table
	df = df.agg(count=('resp_body', px.count))
	px.display(df)

Args:
	table (str): the name of the table to load into the dataframe.
	select (List[str]): the list of columns to select. If empty,
		all columns are selected.
	start_time (px.Time): the timestamp of data to start loading data.
	stop_time (px.Time): the timestamp of data which to stop loading data.

Returns:
	DataFrame: table loaded as a DataFrame`

// Valid doc strings
const twoSpaceDataFrameDoc = `DataFrame represents processed data in Pxl

DataFrame is the primary data structure in Pxl. It exposes data transformation operations through its API.
Examples:
  # Create a dataframe object
  df = px.DataFrame('http_events', start_time='-5m')
  # Aggregate to calculate number of events in the http_events table
  df = df.agg(count=('resp_body', px.count))
  px.display(df)


Args:
  table (str): the name of the table to load into the dataframe.
  select (List[str]): the list of columns to select. If empty,
    all columns are selected.
  start_time (px.Time): the timestamp of data to start loading data.
  stop_time (px.Time): the timestamp of data which to stop loading data.

Returns:
  DataFrame: table loaded as a DataFrame`

const fourSpaceDataFrameDoc = `DataFrame represents processed data in Pxl

DataFrame is the primary data structure in Pxl. It exposes data transformation operations through its API.
Examples:
    # Create a dataframe object
    df = px.DataFrame('http_events', start_time='-5m')
    # Aggregate to calculate number of events in the http_events table
    df = df.agg(count=('resp_body', px.count))
    px.display(df)


Args:
    table (str): the name of the table to load into the dataframe.
    select (List[str]): the list of columns to select. If empty,
        all columns are selected.
    start_time (px.Time): the timestamp of data to start loading data.
    stop_time (px.Time): the timestamp of data which to stop loading data.

Returns:
    DataFrame: table loaded as a DataFrame`

const mismatchTabsDataFrameDoc = `DataFrame represents processed data in Pxl

DataFrame is the primary data structure in Pxl. It exposes data transformation operations through its API.
Examples:
		# Create a dataframe object
    df = px.DataFrame('http_events', start_time='-5m')
    # Aggregate to calculate number of events in the http_events table
    df = df.agg(count=('resp_body', px.count))
    px.display(df)


Args:
    table (str): the name of the table to load into the dataframe.
    select (List[str]): the list of columns to select. If empty,
        all columns are selected.
    start_time (px.Time): the timestamp of data to start loading data.
    stop_time (px.Time): the timestamp of data which to stop loading data.

Returns:
    DataFrame: table loaded as a DataFrame`

const upsertTracepointDoc = `
	Upserts a tracepoint on the UPID and writes results to the table.

	Upserts the passed in tracepoint on the UPID. Each tracepoint is unique
	by name. If you upsert on the same trace name and use the same probe func,
	the TTL of that probe function should update. If you upsert on the same
	trace name and different probe func, deploying the probe should fail. If you
	try to write to the same table with a different output schema

	:topic: pixie_state_management

	Args:
		name (str): The name of the tracepoint. Should be unique with the probe_fn.
		table_name (str): The table name to write the results. If the schema output
			by the probe function does not match, then the tracepoint manager will
			error out.
		probe_fn (px.ProbeFn): The probe function to use as part of the Upsert. The return
			value should bhet
		upid (px.UPID): The program to trace as specified by unique Vizier PID.
		ttl (px.Duration): The length of time that a tracepoint will stay alive, after
			which it will be removed.`

const aggDoc = `Aggregates using specified operations

Args:
	**kwargs (Tuple[string, FunctionType]): keys are the column names
		The values are a 2 element tuple containing the column name
		and UDA to apply to the column name.

Returns:
	DataFrame: Dataframe with any grouped columns followed by the aggregated columns `

const docDrop = `Drop the specified columns from the DataFrame

Args:
	columns (Union[string, List[string]]): either a string or a list of strings representing columns.
		If the column doesn't exist, will throw an error.

Returns:
	DataFrame: DataFrame with the specified columns removed.`

const longReturnDesc = `Drop the specified columns from the DataFrame

Args:
	columns (Union[string, List[string]]): either a string or a list of strings representing columns.
		If the column doesn't exist, will throw an error.

Returns:
	DataFrame: DataFrame with the specified columns removed.

	More documentation here about the return.
	`

const docReturnBeforeArgs = `short desc

long desc that can span many lines and even have a paragraph break.
I love long descriptions.

Wow we can even support another paragraph

Returns:
	DataFrame: The DataFrame containing the memory source.

Args:
	arg (str): first line of description should be like this.
		Second line should be like this.
		Third line should also be like this.

	arg2 (str): second arg can be separated by newline.
`

// Invalid Docstrings
const docWtihBadArgContinuation = `short desc

long desc

Args:
	arg (str): first line of description should be like this
	Second description line should be indented, they are not.
`

const docHeaderButNoBody = `short desc

long desc

Args:
`

const docHeaderMashUp1 = `short desc

long desc

Args:andsomemorethat should cause this to fail
`

const docHeaderMashUp2 = `short desc

long desc

heyweretheArgs:andsomemorethat should cause this to fail
`

const docIndentedAndArgsBadIndent = `
  Decorates a tracepoint definition of a Go function.

  Specifies the decorated function as a probe tracepoint on the 'trace_fn'
  name.

  the next line is bad
  Args:
  trace_fn (str): The Go func to trace. Format is '<package_name>.<func_name>'.

  Returns:
    Func: The wrapped probe function.
  )doc";
`

func TestParseDocstring(t *testing.T) {
	// DataFrame documentation.
	parsedDoc, err := parseDocstring(dataFrameDoc)
	require.NoError(t, err)

	assert.ElementsMatch(t, parsedDoc.function.ReturnType.Types, []string{"DataFrame"})
	assert.Regexp(t, "table loaded as a DataFrame", parsedDoc.function.ReturnType.Desc)

	require.Equal(t, 4, len(parsedDoc.function.Args))

	assert.Equal(t, parsedDoc.function.Args[0].Ident, "table")
	assert.ElementsMatch(t, parsedDoc.function.Args[0].Types, []string{"str"})
	assert.Regexp(t, "the name of the table to load", parsedDoc.function.Args[0].Desc)
	assert.Equal(t, parsedDoc.function.Args[1].Ident, "select")
	assert.Equal(t, parsedDoc.function.Args[2].Ident, "start_time")
	assert.Equal(t, parsedDoc.function.Args[3].Ident, "stop_time")

	// Check examples.
	require.Len(t, parsedDoc.body.Examples, 1)
	// Is the example wrapped by backticks?
	assert.Regexp(t, "(?s)^```.*```$", parsedDoc.body.Examples[0].Value)

	// Aggregate document with kwargs.
	parsedDoc, err = parseDocstring(aggDoc)
	require.NoError(t, err)
	require.Equal(t, 1, len(parsedDoc.function.Args))

	assert.Regexp(t, "keys are the column names", parsedDoc.function.Args[0].Desc)
	assert.ElementsMatch(t, parsedDoc.function.Args[0].Types, []string{"Tuple[string, FunctionType]"})

	// Return documentation.
	parsedDoc, err = parseDocstring(docReturnBeforeArgs)
	require.NoError(t, err)

	assert.ElementsMatch(t, parsedDoc.function.ReturnType.Types, []string{"DataFrame"})
	assert.Regexp(t, "containing the memory source", parsedDoc.function.ReturnType.Desc)

	require.Equal(t, len(parsedDoc.function.Args), 2)
	assert.Equal(t, parsedDoc.function.Args[0].Ident, "arg")
	assert.Equal(t, parsedDoc.function.Args[1].Ident, "arg2")

	// Drop documentation.
	parsedDoc, err = parseDocstring(docDrop)
	require.NoError(t, err)

	assert.ElementsMatch(t, parsedDoc.function.ReturnType.Types, []string{"DataFrame"})
	assert.Regexp(t, "columns removed", parsedDoc.function.ReturnType.Desc)

	require.Equal(t, len(parsedDoc.function.Args), 1)
	assert.Equal(t, parsedDoc.function.Args[0].Ident, "columns")

	parsedDoc, err = parseDocstring(longReturnDesc)
	require.NoError(t, err)
	assert.Regexp(t, "More documentation here.", parsedDoc.function.ReturnType.Desc)
}

func TestFailures(t *testing.T) {
	_, err := parseDocstring(docWtihBadArgContinuation)
	assert.Error(t, err)
	assert.Regexp(t, "Unexpected indent", err)

	_, err = parseDocstring(docHeaderButNoBody)
	assert.Error(t, err)
	assert.Regexp(t, "Expected arg description", err)

	parsedDoc, err := parseDocstring(docHeaderMashUp1)
	require.NoError(t, err)
	assert.Equal(t, 0, len(parsedDoc.function.Args))

	parsedDoc, err = parseDocstring(docHeaderMashUp2)
	require.NoError(t, err)

	assert.Equal(t, 0, len(parsedDoc.function.Args))

	_, err = parseDocstring(strings.TrimSpace(dedent(docIndentedAndArgsBadIndent)))
	assert.Regexp(t, "Expected arg description", err)
}

func TestParseAllDocStrings(t *testing.T) {
	internalDoc := &docspb.InternalPXLDocs{}
	internalDoc.UdfDocs = &udfspb.Docs{}
	internalDoc.DocstringNodes = []*docspb.DocstringNode{
		{
			Name:      "px",
			Docstring: "",
			Children: []*docspb.DocstringNode{
				// Should show up in the module.
				{
					Name:      "UpsertTracepoint",
					Docstring: upsertTracepointDoc,
				},
				// Should not show up.
				{
					Name:      "DataFrame",
					Docstring: dataFrameDoc,
				},
			},
		},
	}

	structDocs, err := ParseAllDocStrings(internalDoc)
	require.NoError(t, err)
	require.Len(t, structDocs.MutationDocs, 1)

	// The topic matcher should push it into the body.
	body := structDocs.MutationDocs[0].Body
	function := structDocs.MutationDocs[0].FuncDoc
	assert.Equal(t, "px.UpsertTracepoint", body.Name)
	assert.Regexp(t, "Upserts a tracepoint on the UPID", body.Brief)
	assert.Regexp(t, "Each tracepoint is unique by name", body.Desc)
	assert.Len(t, function.Args, 5)
	// Description should not contain the topic, but should be in the original docstring.
	assert.Regexp(t, ":topic:", upsertTracepointDoc)
	assert.NotRegexp(t, ":topic:", body.Desc)

	// Make sure the other categories are not populated.
	assert.Len(t, structDocs.TracepointDecoratorDocs, 0)
	assert.Len(t, structDocs.TracepointFieldDocs, 0)
}

func TestGetTabChar(t *testing.T) {
	tabChar, err := getTabChar(dataFrameDoc)
	require.NoError(t, err)
	assert.Equal(t, "\t", tabChar)

	tabChar, err = getTabChar(twoSpaceDataFrameDoc)
	require.NoError(t, err)
	assert.Equal(t, "  ", tabChar)

	tabChar, err = getTabChar(fourSpaceDataFrameDoc)
	require.NoError(t, err)
	assert.Equal(t, "    ", tabChar)

	_, err = getTabChar(mismatchTabsDataFrameDoc)
	require.Error(t, err)
	assert.Regexp(t, "Mismatching tabChars", err.Error())
}

func TestDedent(t *testing.T) {
	// First make sure that tracepoint doesn't work without processing.
	_, err := parseDocstring(upsertTracepointDoc)
	assert.Error(t, err)

	// Dedent it and make sure it's actually changed.
	dedented := dedent(upsertTracepointDoc)
	assert.NotEqual(t, dedented, upsertTracepointDoc)

	// Now that it's dedented, we should be able to parse it
	funcDoc, err := parseDocstring(strings.TrimSpace(dedented))
	require.NoError(t, err)
	// Check that we actually parsed it.
	assert.Regexp(t, "Upserts a tracepoint on the UPID", funcDoc.body.Brief)
	assert.Len(t, funcDoc.function.Args, 5)

	// Make sure it doesn't dedent stuff that doesn't need dedenting.
	assert.Equal(t, dataFrameDoc, dedent(dataFrameDoc))
	assert.Equal(t, twoSpaceDataFrameDoc, dedent(twoSpaceDataFrameDoc))
	assert.Equal(t, fourSpaceDataFrameDoc, dedent(fourSpaceDataFrameDoc))
}
