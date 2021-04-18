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

package datastore_test

import (
	"fmt"
	"os"
	"testing"

	"github.com/gofrs/uuid"
	_ "github.com/golang-migrate/migrate/source/go_bindata"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	_ "github.com/jackc/pgx/stdlib"
	"github.com/jmoiron/sqlx"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/project_manager/datastore"
	"px.dev/pixie/src/cloud/project_manager/schema"
	"px.dev/pixie/src/shared/services/pgtest"
)

var testOrgID1 = uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000")
var testOrgID2 = uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000")

func mustLoadTestData(db *sqlx.DB) {
	db.MustExec(`DELETE from projects`)

	insertQuery := `INSERT INTO projects (org_id, project_name) VALUES ($1, $2)`
	db.MustExec(insertQuery, testOrgID1.String(), "default")
	db.MustExec(insertQuery, testOrgID2.String(), "default")
}

func TestMain(m *testing.M) {
	err := testMain(m)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Got error: %v\n", err)
		os.Exit(1)
	}
	os.Exit(0)
}

var db *sqlx.DB

func testMain(m *testing.M) error {
	s := bindata.Resource(schema.AssetNames(), schema.Asset)
	testDB, teardown, err := pgtest.SetupTestDB(s)
	if err != nil {
		return fmt.Errorf("failed to start test database: %w", err)
	}

	defer teardown()
	db = testDB

	if c := m.Run(); c != 0 {
		return fmt.Errorf("some tests failed with code: %d", c)
	}
	return nil
}

func TestDatastore(t *testing.T) {
	mustLoadTestData(db)

	tests := []struct {
		name        string
		projectName string
		available   bool
	}{
		{"project in database", "default", false},
		{"project not in database", "domain2", true},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			datastore, err := datastore.NewDatastore(db)
			require.NoError(t, err)
			available, err := datastore.CheckAvailability(testOrgID1, test.projectName)
			require.NoError(t, err)
			assert.Equal(t, available, test.available)
		})
	}

	// Insert into datastore and check if there.
	t.Run("Register and Check", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.NoError(t, err)
		available, err := datastore.CheckAvailability(testOrgID1, "my domain")
		require.NoError(t, err)
		assert.True(t, available)
		err = datastore.RegisterProject(testOrgID1, "my-domain")
		require.NoError(t, err)
		available, err = datastore.CheckAvailability(testOrgID1, "my-domain")
		require.NoError(t, err)
		assert.False(t, available)
	})

	t.Run("Registering existing project should fail", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.NoError(t, err)
		err = datastore.RegisterProject(testOrgID1, "default")
		assert.NotNil(t, err)
	})

	t.Run("Get project by name for existing project", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.NoError(t, err)
		projectInfo, err := datastore.GetProjectByName(testOrgID2, "default")
		require.NoError(t, err)
		require.NotNil(t, projectInfo)

		assert.Equal(t, projectInfo.OrgID, testOrgID2)
		assert.Equal(t, projectInfo.ProjectName, "default")
	})

	t.Run("Get project by name for missing project", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.NoError(t, err)
		projectInfo, err := datastore.GetProjectByName(testOrgID1, "h")
		require.NoError(t, err)
		assert.Nil(t, projectInfo)
	})

	t.Run("Get project by name for empty project", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.NoError(t, err)
		projectInfo, err := datastore.GetProjectByName(testOrgID1, "")
		require.NoError(t, err)
		assert.Nil(t, projectInfo)
	})

	t.Run("Get project by org", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.NoError(t, err)
		projectInfo, err := datastore.GetProjectForOrg(testOrgID1)
		require.NoError(t, err)
		require.NotNil(t, projectInfo)

		assert.Equal(t, projectInfo.OrgID, testOrgID1)
		assert.Equal(t, projectInfo.ProjectName, "default")
	})

	t.Run("Get project by org for missing org", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.NoError(t, err)
		projectInfo, err := datastore.GetProjectForOrg(uuid.FromStringOrNil("623e4567-e89b-12d3-a456-426655440010"))
		require.NoError(t, err)
		require.Nil(t, projectInfo)
	})
}
