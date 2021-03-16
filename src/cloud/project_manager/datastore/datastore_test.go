package datastore_test

import (
	"testing"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"

	"github.com/gofrs/uuid"
	_ "github.com/golang-migrate/migrate/source/go_bindata"
	_ "github.com/jackc/pgx/stdlib"
	"github.com/jmoiron/sqlx"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"pixielabs.ai/pixielabs/src/cloud/project_manager/datastore"
	"pixielabs.ai/pixielabs/src/cloud/project_manager/schema"
	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
)

var testOrgID1 = uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000")
var testOrgID2 = uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000")

func loadTestData(t *testing.T, db *sqlx.DB) {
	insertQuery := `INSERT INTO projects (org_id, project_name) VALUES ($1, $2)`
	db.MustExec(insertQuery, testOrgID1.String(), "default")
	db.MustExec(insertQuery, testOrgID2.String(), "default")
}

func TestDatastore(t *testing.T) {
	s := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})
	db, teardown := pgtest.SetupTestDB(t, s)
	defer teardown()

	loadTestData(t, db)

	require.NotNil(t, db)

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
			require.Nil(t, err)
			available, err := datastore.CheckAvailability(testOrgID1, test.projectName)
			assert.Nil(t, err)
			assert.Equal(t, available, test.available)
		})
	}

	// Insert into datastore and check if there.
	t.Run("Register and Check", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		available, err := datastore.CheckAvailability(testOrgID1, "my domain")
		require.Nil(t, err)
		assert.True(t, available)
		err = datastore.RegisterProject(testOrgID1, "my-domain")
		require.Nil(t, err)
		available, err = datastore.CheckAvailability(testOrgID1, "my-domain")
		require.Nil(t, err)
		assert.False(t, available)
	})

	t.Run("Registering existing project should fail", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		err = datastore.RegisterProject(testOrgID1, "default")
		assert.NotNil(t, err)
	})

	t.Run("Get project by name for existing project", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		projectInfo, err := datastore.GetProjectByName(testOrgID2, "default")
		require.Nil(t, err)
		require.NotNil(t, projectInfo)

		assert.Equal(t, projectInfo.OrgID, testOrgID2)
		assert.Equal(t, projectInfo.ProjectName, "default")
	})

	t.Run("Get project by name for missing project", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		projectInfo, err := datastore.GetProjectByName(testOrgID1, "h")
		assert.Nil(t, err)
		assert.Nil(t, projectInfo)
	})

	t.Run("Get project by name for empty project", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		projectInfo, err := datastore.GetProjectByName(testOrgID1, "")
		assert.Nil(t, err)
		assert.Nil(t, projectInfo)
	})

	t.Run("Get project by org", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		projectInfo, err := datastore.GetProjectForOrg(testOrgID1)
		require.Nil(t, err)
		require.NotNil(t, projectInfo)

		assert.Equal(t, projectInfo.OrgID, testOrgID1)
		assert.Equal(t, projectInfo.ProjectName, "default")
	})

	t.Run("Get project by org for missing org", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		projectInfo, err := datastore.GetProjectForOrg(uuid.FromStringOrNil("623e4567-e89b-12d3-a456-426655440010"))
		require.Nil(t, err)
		require.Nil(t, projectInfo)
	})
}
