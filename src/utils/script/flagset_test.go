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

package script_test

import (
	"errors"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/utils/script"
)

func setupTest() *script.FlagSet {
	flags := script.NewFlagSet("px/cluster")

	def1 := "abc"
	def2 := "def"

	flags.String("no_default1", nil, "a flag with no default")
	flags.String("no_default2", nil, "another flag with no default")
	flags.String("f3", &def1, "flag with a default value 1")
	flags.String("f4", &def2, "flag with a default value 2")
	return flags
}

func TestParseFlags(t *testing.T) {
	flags := setupTest()

	flagVals := []string{
		"-no_default1=true",
		"--no_default2=\"\"",
		"-f3", "5",
		"--f4", "6",
	}

	assert.Nil(t, flags.Parse(flagVals))

	f1, err := flags.Lookup("no_default1")
	require.NoError(t, err)
	assert.Equal(t, f1, "true")

	f2, err := flags.Lookup("no_default2")
	require.NoError(t, err)
	assert.Equal(t, f2, "\"\"")

	f3, err := flags.Lookup("f3")
	require.NoError(t, err)
	assert.Equal(t, f3, "5")

	f4, err := flags.Lookup("f4")
	require.NoError(t, err)
	assert.Equal(t, f4, "6")
}

func TestSetFlag(t *testing.T) {
	flags := setupTest()

	flagVals := []string{
		"-no_default1=true",
		"--no_default2=\"\"",
		"-f3", "5",
		"--f4", "6",
	}

	assert.Nil(t, flags.Parse(flagVals))

	err := flags.Set("f3", "555")
	require.NoError(t, err)

	f3, err := flags.Lookup("f3")
	require.NoError(t, err)
	assert.Equal(t, f3, "555")
}

func TestMissingRequiredFlags(t *testing.T) {
	flags := setupTest()

	flagVals := []string{
		"--no_default2=\"\"",
		"-f3", "5",
		"--f4", "6",
	}

	assert.Nil(t, flags.Parse(flagVals))

	f1, err := flags.Lookup("no_default1")
	assert.NotNil(t, err)
	assert.True(t, errors.Is(err, script.ErrMissingRequiredArgument))
	assert.Equal(t, f1, "")

	f2, err := flags.Lookup("no_default2")
	require.NoError(t, err)
	assert.Equal(t, f2, "\"\"")

	f3, err := flags.Lookup("f3")
	require.NoError(t, err)
	assert.Equal(t, f3, "5")

	f4, err := flags.Lookup("f4")
	require.NoError(t, err)
	assert.Equal(t, f4, "6")
}
