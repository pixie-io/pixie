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

	insertUserSetting := `INSERT INTO user_settings (user_id, key, value) VALUES ($1, $2, $3)`
	db.MustExec(insertUserSetting, "123e4567-e89b-12d3-a456-426655440001", "some_setting", "test")
	db.MustExec(insertUserSetting, "123e4567-e89b-12d3-a456-426655440001", "another_setting", "true")
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

	t.Run("get orgs", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		orgs, err := d.GetOrgs()
		require.Nil(t, err)
		require.NotNil(t, orgs)

		assert.Equal(t, 1, len(orgs))
		assert.Equal(t, "hulu", orgs[0].OrgName)
		assert.Equal(t, "hulu.com", orgs[0].DomainName)
	})

	t.Run("get org by domain for missing domain should a specific error", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		orgInfo, err := d.GetOrgByDomain("goo.com")
		require.NotNil(t, err)
		require.Equal(t, err, datastore.ErrOrgNotFound)
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

	t.Run("get user by email", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		userInfo, err := d.GetUserByEmail("person@hulu.com")
		require.Nil(t, err)
		require.NotNil(t, userInfo)

		assert.Equal(t, userInfo.Email, "person@hulu.com")
	})

	t.Run("get user by email for missing email should return specific error", func(t *testing.T) {
		d := datastore.NewDatastore(db)
		userInfo, err := d.GetUserByEmail("noemail@gmail.com")
		require.NotNil(t, err)
		require.Equal(t, err, datastore.ErrUserNotFound)
		require.Nil(t, userInfo)
	})

	t.Run("delete org and its users", func(t *testing.T) {
		db, teardown := pgtest.SetupTestDB(t, s)
		defer teardown()

		loadTestData(t, db)

		orgID := "223e4567-e89b-12d3-a456-426655440009"
		userID := "223e4567-e89b-12d3-a456-426655440001"

		// Add in data to be deleted
		insertOrgQuery := `INSERT INTO orgs (id, org_name, domain_name) VALUES ($1, $2, $3)`
		db.MustExec(insertOrgQuery, orgID, "not-hulu", "not-hulu.com")
		insertUserQuery := `INSERT INTO users (id, org_id, username, first_name, last_name, email) VALUES ($1, $2, $3, $4, $5, $6)`
		db.MustExec(insertUserQuery, userID, orgID, "person@not-hulu.com", "first", "last", "person@not-hulu.com")

		d := datastore.NewDatastore(db)

		// Should show up before the deletion
		userInfo, err := d.GetUser(uuid.FromStringOrNil(userID))
		require.Nil(t, err)
		require.NotNil(t, userInfo)
		orgInfo, err := d.GetOrg(uuid.FromStringOrNil(orgID))
		require.Nil(t, err)
		require.NotNil(t, orgInfo)

		err = d.DeleteOrgAndUsers(uuid.FromStringOrNil(orgID))
		require.Nil(t, err)

		// Should not show up before the deletion
		userInfo, err = d.GetUser(uuid.FromStringOrNil(userID))
		require.NotNil(t, err)
		require.Nil(t, userInfo)
		orgInfo, err = d.GetOrg(uuid.FromStringOrNil(orgID))
		require.NotNil(t, err)
		require.Nil(t, orgInfo)
	})

	t.Run("update user", func(t *testing.T) {
		db, teardown := pgtest.SetupTestDB(t, s)
		defer teardown()

		loadTestData(t, db)
		d := datastore.NewDatastore(db)

		userID := "123e4567-e89b-12d3-a456-426655440001"
		profilePicture := "http://somepicture"
		err := d.UpdateUser(&datastore.UserInfo{ID: uuid.FromStringOrNil(userID), FirstName: "first", LastName: "last", ProfilePicture: &profilePicture})
		assert.Nil(t, err)

		userInfoFetched, err := d.GetUser(uuid.FromStringOrNil(userID))
		require.Nil(t, err)
		require.NotNil(t, userInfoFetched)
		assert.Equal(t, "http://somepicture", *userInfoFetched.ProfilePicture)

	})

	t.Run("Get user settings", func(t *testing.T) {
		db, teardown := pgtest.SetupTestDB(t, s)
		defer teardown()

		loadTestData(t, db)
		d := datastore.NewDatastore(db)

		userSettingsFetched, err := d.GetUserSettings(uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440001"), []string{"another_setting", "doesnt_exist", "some_setting"})
		assert.Nil(t, err)
		assert.Equal(t, []string{"true", "", "test"}, userSettingsFetched)
	})

	t.Run("Update user settings", func(t *testing.T) {
		db, teardown := pgtest.SetupTestDB(t, s)
		defer teardown()

		id := "123e4567-e89b-12d3-a456-426655440001"

		loadTestData(t, db)
		d := datastore.NewDatastore(db)

		err := d.UpdateUserSettings(uuid.FromStringOrNil(id), []string{"new_setting", "another_setting"}, []string{"some_val", "new_value"})
		assert.Nil(t, err)

		query := `SELECT * from user_settings WHERE user_id=$1 AND key=$2`
		expectedKeyValues := [][]string{{"new_setting", "some_val"}, {"another_setting", "new_value"}, {"some_setting", "test"}}

		for _, kv := range expectedKeyValues {
			rows, err := db.Queryx(query, id, kv[0])
			assert.Nil(t, err)
			defer rows.Close()
			var userSetting datastore.UserSetting
			assert.True(t, rows.Next())
			err = rows.StructScan(&userSetting)
			assert.Nil(t, err)
			assert.Equal(t, kv[1], userSetting.Value)
		}
	})
}
