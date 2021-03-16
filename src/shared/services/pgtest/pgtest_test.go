package pgtest_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
)

func TestSetupTestDB(t *testing.T) {
	db, teardown := pgtest.SetupTestDB(t, nil)

	require.NotNil(t, db)
	require.NotNil(t, teardown)
	assert.Nil(t, db.Ping())

	teardown()

	// This should fail b/c database should be closed after teardown.
	err := db.Ping()
	assert.NotNil(t, err)
}
