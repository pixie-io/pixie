package datastore_test

import (
	"testing"

	_ "github.com/golang-migrate/migrate/source/go_bindata"
	_ "github.com/jackc/pgx/stdlib"
	"github.com/jmoiron/sqlx"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"pixielabs.ai/pixielabs/src/services/site_manager/datastore"
	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
)

func loadTestData(t *testing.T, db *sqlx.DB) {
	insertQuery := `INSERT INTO sites (org_id, domain_name) VALUES ($1, $2)`
	db.MustExec(insertQuery, "123e4567-e89b-12d3-a456-426655440000", "hulu")
	db.MustExec(insertQuery, "223e4567-e89b-12d3-a456-426655440000", "thousand-eyes")
}

func TestDatastore(t *testing.T) {
	db, teardown := pgtest.SetupTestDB(t)
	defer teardown()

	loadTestData(t, db)

	require.NotNil(t, db)

	tests := []struct {
		name       string
		domainName string
		available  bool
	}{
		{"domain in database", "hulu", false},
		{"domain not in database", "domain2", true},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			datastore, err := datastore.NewDatastore(db)
			require.Nil(t, err)
			available, err := datastore.CheckAvailability(test.domainName)
			assert.Nil(t, err)
			assert.Equal(t, available, test.available)
		})
	}

	// Insert into datastore and check if there.
	t.Run("Register and Check", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		available, err := datastore.CheckAvailability("my domain")
		require.Nil(t, err)
		assert.True(t, available)
		err = datastore.RegisterSite(uuid.UUID{}, "my-domain")
		require.Nil(t, err)
		available, err = datastore.CheckAvailability("my-domain")
		require.Nil(t, err)
		assert.False(t, available)
	})

	t.Run("Registering existing site should fail", func(t *testing.T) {
		datastore, err := datastore.NewDatastore(db)
		require.Nil(t, err)
		err = datastore.RegisterSite(uuid.UUID{}, "hulu")
		assert.NotNil(t, err)
	})
}
