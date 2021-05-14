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
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/profile/datastore"
	"px.dev/pixie/src/cloud/profile/schema"
	"px.dev/pixie/src/shared/services/pgtest"
)

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

func mustLoadTestData(db *sqlx.DB) {
	// Cleanup.
	db.MustExec(`DELETE FROM user_settings`)
	db.MustExec(`DELETE FROM users`)
	db.MustExec(`DELETE FROM orgs`)

	insertOrgQuery := `INSERT INTO orgs (id, org_name, domain_name, enable_approvals) VALUES ($1, $2, $3, $4)`
	db.MustExec(insertOrgQuery, "123e4567-e89b-12d3-a456-426655440000", "my-org", "my-org.com", "false")
	insertUserQuery := `INSERT INTO users (id, org_id, username, first_name, last_name, email, is_approved) VALUES ($1, $2, $3, $4, $5, $6, $7)`
	db.MustExec(insertUserQuery, "123e4567-e89b-12d3-a456-426655440001", "123e4567-e89b-12d3-a456-426655440000", "person@my-org.com", "first", "last", "person@my-org.com", "true")
	db.MustExec(insertUserQuery, "123e4567-e89b-12d3-a456-426655440002", "123e4567-e89b-12d3-a456-426655440000", "person2@my-org.com", "first2", "last2", "person2@my-org.com", "false")

	insertUserSetting := `INSERT INTO user_settings (user_id, key, value) VALUES ($1, $2, $3)`
	db.MustExec(insertUserSetting, "123e4567-e89b-12d3-a456-426655440001", "some_setting", "test")
	db.MustExec(insertUserSetting, "123e4567-e89b-12d3-a456-426655440001", "another_setting", "true")
}

func TestDatastore(t *testing.T) {
	t.Run("test insert and get user", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		userInfo := datastore.UserInfo{
			OrgID:     uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			Username:  "zain",
			FirstName: "zain",
			LastName:  "asgar",
			Email:     "zasgar@pixielabs.ai",
		}
		userID, err := d.CreateUser(&userInfo)
		require.NoError(t, err)
		require.NotEqual(t, userID, uuid.Nil)

		userInfoFetched, err := d.GetUser(userID)
		require.NoError(t, err)
		require.NotNil(t, userInfoFetched)

		assert.Equal(t, userID, userInfoFetched.ID)
		assert.Equal(t, userInfo.OrgID, userInfoFetched.OrgID)
		assert.Equal(t, userInfo.Username, userInfoFetched.Username)
		assert.Equal(t, userInfo.FirstName, userInfoFetched.FirstName)
		assert.Equal(t, userInfo.LastName, userInfoFetched.LastName)
		assert.Equal(t, userInfo.Email, userInfoFetched.Email)
	})

	t.Run("inserting existing user should fail", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		userInfo := datastore.UserInfo{
			OrgID:     uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"),
			Username:  "person@my-org.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "person@my-org.com",
		}
		userID, err := d.CreateUser(&userInfo)
		assert.NotNil(t, err)
		assert.Equal(t, userID, uuid.Nil)
	})

	t.Run("insert user with bad org should fail", func(t *testing.T) {
		mustLoadTestData(db)
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
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		orgInfo, err := d.GetOrg(uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"))
		require.NoError(t, err)
		require.NotNil(t, orgInfo)

		assert.Equal(t, orgInfo.ID, uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"))
		assert.Equal(t, orgInfo.DomainName, "my-org.com")
		assert.Equal(t, orgInfo.OrgName, "my-org")
	})

	t.Run("get org by domain", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		orgInfo, err := d.GetOrgByDomain("my-org.com")
		require.NoError(t, err)
		require.NotNil(t, orgInfo)

		assert.Equal(t, orgInfo.OrgName, "my-org")
		assert.Equal(t, orgInfo.DomainName, "my-org.com")
	})

	t.Run("get orgs", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		orgs, err := d.GetOrgs()
		require.NoError(t, err)
		require.NotNil(t, orgs)

		assert.Equal(t, 1, len(orgs))
		assert.Equal(t, "my-org", orgs[0].OrgName)
		assert.Equal(t, "my-org.com", orgs[0].DomainName)
	})

	t.Run("get org by domain for missing domain should a specific error", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		orgInfo, err := d.GetOrgByDomain("goo.com")
		require.NotNil(t, err)
		require.Equal(t, err, datastore.ErrOrgNotFound)
		require.Nil(t, orgInfo)
	})

	t.Run("create org and user first time user", func(t *testing.T) {
		mustLoadTestData(db)
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
		require.NoError(t, err)
		assert.NotEqual(t, orgID, uuid.Nil)
		assert.NotEqual(t, userID, uuid.Nil)

		assert.Equal(t, orgInfo.ID, orgID)
		assert.Equal(t, userInfo.ID, userID)
		assert.Equal(t, userInfo.OrgID, orgID)
	})

	t.Run("create org and user first time user case should fail for existing org", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		orgInfo := datastore.OrgInfo{
			OrgName:    "my-org",
			DomainName: "my-org.com",
		}
		userInfo := datastore.UserInfo{
			Username:  "johnd",
			FirstName: "john",
			LastName:  "doe",
			Email:     "john@my-org.com",
		}

		orgID, userID, err := d.CreateUserAndOrg(&orgInfo, &userInfo)
		require.NotNil(t, err)
		assert.Equal(t, orgID, uuid.Nil)
		assert.Equal(t, userID, uuid.Nil)
	})

	t.Run("create org and user should fail for existing user", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		orgInfo := datastore.OrgInfo{
			OrgName:    "my-org",
			DomainName: "my-org.com",
		}
		userInfo := datastore.UserInfo{
			Username:  "person@my-org.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "person@my-org.com",
		}

		orgID, userID, err := d.CreateUserAndOrg(&orgInfo, &userInfo)
		require.NotNil(t, err)
		assert.Equal(t, orgID, uuid.Nil)
		assert.Equal(t, userID, uuid.Nil)
	})

	t.Run("get user by email", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		userInfo, err := d.GetUserByEmail("person@my-org.com")
		require.NoError(t, err)
		require.NotNil(t, userInfo)

		assert.Equal(t, userInfo.Email, "person@my-org.com")
	})

	t.Run("get user by email for missing email should return specific error", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)
		userInfo, err := d.GetUserByEmail("noemail@gmail.com")
		require.NotNil(t, err)
		require.Equal(t, err, datastore.ErrUserNotFound)
		require.Nil(t, userInfo)
	})

	t.Run("delete org and its users", func(t *testing.T) {
		mustLoadTestData(db)
		orgID := "223e4567-e89b-12d3-a456-426655440009"
		userID := "223e4567-e89b-12d3-a456-426655440001"

		// Add in data to be deleted
		insertOrgQuery := `INSERT INTO orgs (id, org_name, domain_name) VALUES ($1, $2, $3)`
		db.MustExec(insertOrgQuery, orgID, "not-my-org", "not-my-org.com")
		insertUserQuery := `INSERT INTO users (id, org_id, username, first_name, last_name, email) VALUES ($1, $2, $3, $4, $5, $6)`
		db.MustExec(insertUserQuery, userID, orgID, "person@not-my-org.com", "first", "last", "person@not-my-org.com")

		d := datastore.NewDatastore(db)

		// Should show up before the deletion
		userInfo, err := d.GetUser(uuid.FromStringOrNil(userID))
		require.NoError(t, err)
		require.NotNil(t, userInfo)
		orgInfo, err := d.GetOrg(uuid.FromStringOrNil(orgID))
		require.NoError(t, err)
		require.NotNil(t, orgInfo)

		err = d.DeleteOrgAndUsers(uuid.FromStringOrNil(orgID))
		require.NoError(t, err)

		// Should not show up before the deletion
		userInfo, err = d.GetUser(uuid.FromStringOrNil(userID))
		require.NotNil(t, err)
		require.Nil(t, userInfo)
		orgInfo, err = d.GetOrg(uuid.FromStringOrNil(orgID))
		require.NotNil(t, err)
		require.Nil(t, orgInfo)
	})

	t.Run("update user", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)

		userID := "123e4567-e89b-12d3-a456-426655440001"
		profilePicture := "http://somepicture"
		// Original should be IsApproved -> true.
		err := d.UpdateUser(&datastore.UserInfo{ID: uuid.FromStringOrNil(userID), FirstName: "first", LastName: "last", ProfilePicture: &profilePicture, IsApproved: false})
		require.NoError(t, err)

		userInfoFetched, err := d.GetUser(uuid.FromStringOrNil(userID))
		require.NoError(t, err)
		require.NotNil(t, userInfoFetched)
		assert.Equal(t, "http://somepicture", *userInfoFetched.ProfilePicture)
		assert.Equal(t, false, userInfoFetched.IsApproved)
	})

	t.Run("Get user settings", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)

		userSettingsFetched, err := d.GetUserSettings(uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440001"), []string{"another_setting", "doesnt_exist", "some_setting"})
		require.NoError(t, err)
		assert.Equal(t, []string{"true", "", "test"}, userSettingsFetched)
	})

	t.Run("Update user settings", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)

		id := "123e4567-e89b-12d3-a456-426655440001"
		err := d.UpdateUserSettings(uuid.FromStringOrNil(id), []string{"new_setting", "another_setting"}, []string{"some_val", "new_value"})
		require.NoError(t, err)

		checkKVInDB := func(k, v string) {
			query := `SELECT * from user_settings WHERE user_id=$1 AND key=$2`
			rows, err := db.Queryx(query, id, k)
			require.NoError(t, err)
			defer rows.Close()
			var userSetting datastore.UserSetting
			assert.True(t, rows.Next())
			err = rows.StructScan(&userSetting)
			require.NoError(t, err)
			assert.Equal(t, v, userSetting.Value)
		}

		expectedKeyValues := map[string]string{
			"new_setting":     "some_val",
			"another_setting": "new_value",
			"some_setting":    "test",
		}

		for k, v := range expectedKeyValues {
			checkKVInDB(k, v)
		}
	})

	t.Run("Get users in org", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)

		users, err := d.GetUsersInOrg(uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000"))
		require.NoError(t, err)
		assert.Equal(t, 2, len(users))
	})

	t.Run("update org", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)

		orgID := "123e4567-e89b-12d3-a456-426655440000"
		require.NoError(t, d.UpdateOrg(&datastore.OrgInfo{
			ID:              uuid.FromStringOrNil(orgID),
			EnableApprovals: true,
		}))

		orgInfoFetched, err := d.GetOrg(uuid.FromStringOrNil(orgID))
		require.NoError(t, err)
		require.NotNil(t, orgInfoFetched)
		assert.True(t, orgInfoFetched.EnableApprovals)
	})

	t.Run("approve all users", func(t *testing.T) {
		mustLoadTestData(db)
		d := datastore.NewDatastore(db)

		orgID := uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440000")
		require.NoError(t, d.ApproveAllOrgUsers(orgID))

		users, err := d.GetUsersInOrg(orgID)
		require.NoError(t, err)
		assert.Equal(t, 2, len(users))
		assert.True(t, users[0].IsApproved)
		assert.True(t, users[1].IsApproved)
	})
}
