package datastore_test

import (
	"testing"

	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"pixielabs.ai/pixielabs/src/cloud/profile/datastore"

	"github.com/jmoiron/sqlx"

	"pixielabs.ai/pixielabs/src/cloud/profile/schema"
	"pixielabs.ai/pixielabs/src/shared/services/pgtest"
)

func loadTestData(t *testing.T, db *sqlx.DB) {
	insertOrgQuery := `INSERT INTO orgs (id, org_name, domain_name) VALUES ($1, $2, $3)`
	db.MustExec(insertOrgQuery, "123e4567-e89b-12d3-a456-426655440000", "hulu", "hulu.com")
	insertUserQuery := `INSERT INTO users (id, org_id, username, first_name, last_name, email) VALUES ($1, $2, $3, $4, $5, $6)`
	db.MustExec(insertUserQuery, "123e4567-e89b-12d3-a456-426655440001", "123e4567-e89b-12d3-a456-426655440000", "person@hulu.com", "first", "last", "person@hulu.com")
}

func TestDatastore(t *testing.T) {
	s := bindata.Resource(schema.AssetNames(), func(name string) (bytes []byte, e error) {
		return schema.Asset(name)
	})
	db, teardown := pgtest.SetupTestDB(t, s)
	defer teardown()

	loadTestData(t, db)

	t.Run("test insert and get user", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		userInfo := datastore.UserInfo{
			OrgID:     uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			Username:  "zain",
			FirstName: "zain",
			LastName:  "asgar",
			Email:     "zasgar@pixielabs.ai",
		}
		userID, err := d.CreateUser(&userInfo)
		require.Nil(t, err)
		require.NotEqual(t, userID, uuid.Nil)

		userInfoFetched, err := d.GetUser(userID)
		require.Nil(t, err)
		require.NotNil(t, userInfoFetched)

		assert.Equal(t, userID, userInfoFetched.ID)
		assert.Equal(t, userInfo.OrgID, userInfoFetched.OrgID)
		assert.Equal(t, userInfo.Username, userInfoFetched.Username)
		assert.Equal(t, userInfo.FirstName, userInfoFetched.FirstName)
		assert.Equal(t, userInfo.LastName, userInfoFetched.LastName)
		assert.Equal(t, userInfo.Email, userInfoFetched.Email)
	})

	t.Run("inserting existing user should fail", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		userInfo := datastore.UserInfo{
			OrgID:     uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			Username:  "person@hulu.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "person@hulu.com",
		}
		userID, err := d.CreateUser(&userInfo)
		assert.NotNil(t, err)
		assert.Equal(t, userID, uuid.Nil)
	})

	t.Run("insert user with bad org should fail", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		userInfo := datastore.UserInfo{
			// Changed 123 to 133.
			OrgID:     uuid.FromStringOrNil("133e4567-e89b-12d3-a456-426655440000"),
			Username:  "zain",
			FirstName: "zain",
			LastName:  "asgar",
			Email:     "zasgar@pixielabs.ai",
		}
		userID, err := d.CreateUser(&userInfo)
		assert.NotNil(t, err)
		assert.Equal(t, userID, uuid.Nil)
	})

	t.Run("test get org", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		orgInfo, err := d.GetOrg(uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"))
		require.Nil(t, err)
		require.NotNil(t, orgInfo)

		assert.Equal(t, orgInfo.ID, uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"))
		assert.Equal(t, orgInfo.DomainName, "hulu.com")
		assert.Equal(t, orgInfo.OrgName, "hulu")
	})

	t.Run("get org by domain", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		orgInfo, err := d.GetOrgByDomain("hulu.com")
		require.Nil(t, err)
		require.NotNil(t, orgInfo)

		assert.Equal(t, orgInfo.OrgName, "hulu")
		assert.Equal(t, orgInfo.DomainName, "hulu.com")
	})

	t.Run("get org by domain for missing domain should return nil", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		orgInfo, err := d.GetOrgByDomain("goo.com")
		require.NotNil(t, err)
		require.Nil(t, orgInfo)
	})

	t.Run("create org and user first time user", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		orgInfo := datastore.OrgInfo{
			OrgName:    "pg",
			DomainName: "pg.com",
		}
		userInfo := datastore.UserInfo{
			Username:  "johnd",
			FirstName: "john",
			LastName:  "doe",
			Email:     "john@pg.com",
		}

		orgID, userID, err := d.CreateUserAndOrg(&orgInfo, &userInfo)
		require.Nil(t, err)
		assert.NotEqual(t, orgID, uuid.Nil)
		assert.NotEqual(t, userID, uuid.Nil)

		assert.Equal(t, orgInfo.ID, orgID)
		assert.Equal(t, userInfo.ID, userID)
		assert.Equal(t, userInfo.OrgID, orgID)
	})

	t.Run("create org and user first time user case should fail for existing org", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		orgInfo := datastore.OrgInfo{
			OrgName:    "hulu",
			DomainName: "hulu.com",
		}
		userInfo := datastore.UserInfo{
			Username:  "johnd",
			FirstName: "john",
			LastName:  "doe",
			Email:     "john@hulu.com",
		}

		orgID, userID, err := d.CreateUserAndOrg(&orgInfo, &userInfo)
		require.NotNil(t, err)
		assert.Equal(t, orgID, uuid.Nil)
		assert.Equal(t, userID, uuid.Nil)
	})

	t.Run("create org and user should fail for existing user", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		orgInfo := datastore.OrgInfo{
			OrgName:    "pg",
			DomainName: "pg.com",
		}
		userInfo := datastore.UserInfo{
			Username:  "person@hulu.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "person@hulu.com",
		}

		orgID, userID, err := d.CreateUserAndOrg(&orgInfo, &userInfo)
		require.NotNil(t, err)
		assert.Equal(t, orgID, uuid.Nil)
		assert.Equal(t, userID, uuid.Nil)
	})
}
