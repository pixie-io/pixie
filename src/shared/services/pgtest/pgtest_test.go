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

package pgtest_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/pgtest"
)

func TestSetupTestDB(t *testing.T) {
	db, teardown, err := pgtest.SetupTestDB(nil)

	require.NoError(t, err)
	require.NotNil(t, db)
	require.NotNil(t, teardown)
	assert.Nil(t, db.Ping())

	teardown()

	// This should fail b/c database should be closed after teardown.
	err = db.Ping()
	require.NotNil(t, err)
}
