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

package datastore

import (
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/gofrs/uuid"
	"github.com/jmoiron/sqlx"
)

// TODO(zasgar): Move these to models ?

// UserInfo tracks information about a specific end-user.
type UserInfo struct {
	ID               uuid.UUID  `db:"id"`
	OrgID            uuid.UUID  `db:"org_id"`
	Username         string     `db:"username"`
	FirstName        string     `db:"first_name"`
	LastName         string     `db:"last_name"`
	Email            string     `db:"email"`
	ProfilePicture   *string    `db:"profile_picture"`
	UpdatedAt        *time.Time `db:"updated_at"`
	CreatedAt        *time.Time `db:"created_at"`
	IsApproved       bool       `db:"is_approved"`
	IdentityProvider string     `db:"identity_provider"`
	AuthProviderID   string     `db:"auth_provider_id"`
}

// OrgInfo tracks information about an organization.
type OrgInfo struct {
	ID              uuid.UUID  `db:"id"`
	OrgName         string     `db:"org_name"`
	DomainName      string     `db:"domain_name"`
	EnableApprovals bool       `db:"enable_approvals"`
	UpdatedAt       *time.Time `db:"updated_at"`
	CreatedAt       *time.Time `db:"created_at"`
}

// Datastore is a postgres backed storage for entities.
type Datastore struct {
	db *sqlx.DB
}

// NewDatastore creates a Datastore.
func NewDatastore(db *sqlx.DB) *Datastore {
	return &Datastore{db: db}
}

var (
	// ErrUserNotFound is used when a user is not found when looking up by a filter condition.
	ErrUserNotFound = fmt.Errorf("user not found")
	// ErrOrgNotFound is used when the org is not found when looking up by a filter condition.
	ErrOrgNotFound = fmt.Errorf("org not found")
	// ErrUserAttributesNotFound is used when no attributes can be found for the given user.
	ErrUserAttributesNotFound = fmt.Errorf("user attributes not found")
	// ErrUserSettingsNotFound is used when no settings can be found for the given user.
	ErrUserSettingsNotFound = fmt.Errorf("user settings not found")
)

// CreateUser creates a new user.
func (d *Datastore) CreateUser(userInfo *UserInfo) (uuid.UUID, error) {
	txn, err := d.db.Beginx()
	if err != nil {
		return uuid.Nil, err
	}
	defer txn.Rollback()

	u, err := d.createUserUsingTxn(txn, userInfo)
	if err != nil {
		return uuid.Nil, err
	}

	err = d.createUserAttributesUsingTxn(txn, u)
	if err != nil {
		return uuid.Nil, err
	}

	err = d.createUserSettingsUsingTxn(txn, u)
	if err != nil {
		return uuid.Nil, err
	}

	return u, txn.Commit()
}

// GetUser gets user information by user ID.
func (d *Datastore) GetUser(id uuid.UUID) (*UserInfo, error) {
	query := `SELECT * from users WHERE id=$1`
	rows, err := d.db.Queryx(query, id)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var userInfo UserInfo
		err := rows.StructScan(&userInfo)
		return &userInfo, err
	}
	return nil, errors.New("failed to read user from database")
}

// CreateOrg creates a new organization, returning the created org ID.
func (d *Datastore) CreateOrg(orgInfo *OrgInfo) (uuid.UUID, error) {
	txn, err := d.db.Beginx()
	if err != nil {
		return uuid.Nil, err
	}
	defer txn.Rollback()

	u, err := d.createOrgUsingTxn(txn, orgInfo)
	if err != nil {
		return uuid.Nil, err
	}
	return u, txn.Commit()
}

// CreateUserAndOrg creates a new user and organization as needed for initial user/org creation.
func (d *Datastore) CreateUserAndOrg(orgInfo *OrgInfo, userInfo *UserInfo) (uuid.UUID, uuid.UUID, error) {
	txn, err := d.db.Beginx()
	if err != nil {
		return uuid.Nil, uuid.Nil, err
	}
	defer txn.Rollback()

	orgID, err := d.createOrgUsingTxn(txn, orgInfo)
	if err != nil {
		return uuid.Nil, uuid.Nil, err
	}
	userInfo.OrgID = orgID
	userID, err := d.createUserUsingTxn(txn, userInfo)
	if err != nil {
		return uuid.Nil, uuid.Nil, err
	}

	err = d.createUserAttributesUsingTxn(txn, userID)
	if err != nil {
		return uuid.Nil, uuid.Nil, err
	}

	err = d.createUserSettingsUsingTxn(txn, userID)
	if err != nil {
		return uuid.Nil, uuid.Nil, err
	}

	orgInfo.ID = orgID
	userInfo.ID = userID
	userInfo.OrgID = orgID

	return orgID, userID, txn.Commit()
}

// GetOrg gets org information by ID.
func (d *Datastore) GetOrg(id uuid.UUID) (*OrgInfo, error) {
	query := `SELECT * from orgs WHERE id=$1`
	rows, err := d.db.Queryx(query, id)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var orgInfo OrgInfo
		err := rows.StructScan(&orgInfo)
		return &orgInfo, err
	}
	return nil, errors.New("failed to get org info from database")
}

// GetOrgs gets all orgs.
func (d *Datastore) GetOrgs() ([]*OrgInfo, error) {
	query := `SELECT * from orgs`
	rows, err := d.db.Queryx(query)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	orgs := make([]*OrgInfo, 0)
	for rows.Next() {
		var orgInfo OrgInfo
		err := rows.StructScan(&orgInfo)
		if err != nil {
			return nil, err
		}
		orgs = append(orgs, &orgInfo)
	}
	return orgs, nil
}

// GetOrgByDomain gets org information by domain.
func (d *Datastore) GetOrgByDomain(domainName string) (*OrgInfo, error) {
	query := `SELECT * from orgs WHERE domain_name=$1`
	rows, err := d.db.Queryx(query, domainName)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var orgInfo OrgInfo
		err := rows.StructScan(&orgInfo)
		return &orgInfo, err
	}
	return nil, ErrOrgNotFound
}

// GetUserByEmail gets user info by email.
func (d *Datastore) GetUserByEmail(email string) (*UserInfo, error) {
	query := `SELECT * from users WHERE email=$1`
	rows, err := d.db.Queryx(query, email)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var userInfo UserInfo
		err := rows.StructScan(&userInfo)
		return &userInfo, err
	}
	return nil, ErrUserNotFound
}

// GetUserByAuthProviderID gets userinfo by auth provider id.
func (d *Datastore) GetUserByAuthProviderID(id string) (*UserInfo, error) {
	query := `SELECT * from users WHERE auth_provider_id=$1`
	rows, err := d.db.Queryx(query, id)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var userInfo UserInfo
		err := rows.StructScan(&userInfo)
		return &userInfo, err
	}
	return nil, ErrUserNotFound
}

// DeleteOrgAndUsers deletes the org and users with a given org ID.
func (d *Datastore) DeleteOrgAndUsers(orgID uuid.UUID) error {
	txn, err := d.db.Beginx()
	if err != nil {
		return err
	}
	defer txn.Rollback()

	deleteUsersQuery := `DELETE FROM users WHERE org_id=$1`
	deleteOrgQuery := `DELETE FROM orgs WHERE id=$1`
	_, err = txn.Exec(deleteUsersQuery, orgID)
	if err != nil {
		return err
	}
	_, err = txn.Exec(deleteOrgQuery, orgID)
	if err != nil {
		return err
	}
	return txn.Commit()
}

func (d *Datastore) createUserUsingTxn(txn *sqlx.Tx, userInfo *UserInfo) (uuid.UUID, error) {
	query := `INSERT INTO users (org_id, username, first_name, last_name, email, is_approved, identity_provider, auth_provider_id) VALUES (:org_id, :username, :first_name, :last_name, :email, :is_approved, :identity_provider, :auth_provider_id) RETURNING id`
	row, err := txn.NamedQuery(query, userInfo)
	if err != nil {
		return uuid.Nil, err
	}
	defer row.Close()

	if row.Next() {
		var id uuid.UUID
		if err := row.Scan(&id); err != nil {
			return uuid.Nil, err
		}
		return id, nil
	}
	return uuid.Nil, errors.New("failed to read user id from the database")
}

func (d *Datastore) createOrgUsingTxn(txn *sqlx.Tx, orgInfo *OrgInfo) (uuid.UUID, error) {
	query := `INSERT INTO orgs (org_name, domain_name) VALUES (:org_name, :domain_name) RETURNING id`
	row, err := txn.NamedQuery(query, orgInfo)
	if err != nil {
		return uuid.Nil, err
	}
	defer row.Close()

	if row.Next() {
		var id uuid.UUID
		if err := row.Scan(&id); err != nil {
			return uuid.Nil, err
		}
		return id, nil
	}
	return uuid.Nil, errors.New("failed to read org id from the database")
}

// GetUsersInOrg gets all users in the given org.
func (d *Datastore) GetUsersInOrg(orgID uuid.UUID) ([]*UserInfo, error) {
	query := `SELECT * from users WHERE org_id=$1 order by created_at desc`
	rows, err := d.db.Queryx(query, orgID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	users := make([]*UserInfo, 0)
	for rows.Next() {
		var uInfo UserInfo
		err := rows.StructScan(&uInfo)
		if err != nil {
			return nil, err
		}
		users = append(users, &uInfo)
	}
	return users, nil
}

// UpdateUser updates the user in the database.
func (d *Datastore) UpdateUser(userInfo *UserInfo) error {
	query := `UPDATE users SET profile_picture = :profile_picture, is_approved = :is_approved WHERE id = :id`
	_, err := d.db.NamedExec(query, userInfo)
	return err
}

// UpdateOrg updates the org in the database.
func (d *Datastore) UpdateOrg(orgInfo *OrgInfo) error {
	query := `UPDATE orgs SET enable_approvals = :enable_approvals WHERE id = :id`
	_, err := d.db.NamedExec(query, orgInfo)
	return err
}

// ApproveAllOrgUsers sets all users is_approved column to true in an org.
func (d *Datastore) ApproveAllOrgUsers(orgID uuid.UUID) error {
	query := `UPDATE users SET is_approved = true WHERE org_id = :org_id`
	_, err := d.db.NamedExec(query, struct {
		OrgID uuid.UUID `db:"org_id"`
	}{
		OrgID: orgID,
	})
	return err
}

// UserSettings is a set of settings for a user.
type UserSettings struct {
	UserID          uuid.UUID `db:"user_id"`
	AnalyticsOptout *bool     `db:"analytics_optout"`
}

// GetUserSettings fetches the settings for the given user.
func (d *Datastore) GetUserSettings(id uuid.UUID) (*UserSettings, error) {
	query := `SELECT * from user_settings WHERE user_id=$1`
	rows, err := d.db.Queryx(query, id)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var userSettings UserSettings
		err := rows.StructScan(&userSettings)
		return &userSettings, err
	}

	return nil, ErrUserSettingsNotFound
}

// UpdateUserSettings updates the user settings for the given user.
func (d *Datastore) UpdateUserSettings(settings *UserSettings) error {
	tx, err := d.db.Beginx()
	if err != nil {
		return err
	}
	defer tx.Rollback()

	cols := []string{}
	params := []string{}

	if settings.AnalyticsOptout != nil {
		cols = append(cols, "analytics_optout")
		params = append(params, ":analytics_optout")
	}

	query := `UPDATE user_settings SET (%s) = (%s) WHERE user_id = :user_id`
	if len(cols) == 1 {
		query = `UPDATE user_settings SET %s = %s WHERE user_id = :user_id`
	}
	_, err = d.db.NamedExec(fmt.Sprintf(query, strings.Join(cols, ","), strings.Join(params, ",")), settings)

	if err != nil {
		return err
	}

	err = tx.Commit()
	if err != nil {
		return err
	}

	return nil
}

// createUserSettingsUsingTxn creates default user attributes for the given user.
func (d *Datastore) createUserSettingsUsingTxn(tx *sqlx.Tx, id uuid.UUID) error {
	query := `INSERT INTO user_settings (user_id) VALUES ($1)`
	_, err := tx.Exec(query, id)

	if err != nil {
		return err
	}

	return nil
}

// UserAttributes is a set of attributes for a user.
type UserAttributes struct {
	UserID   uuid.UUID `db:"user_id"`
	TourSeen *bool     `db:"tour_seen"`
}

// GetUserAttributes fetches the settings for the given user and keys.
func (d *Datastore) GetUserAttributes(id uuid.UUID) (*UserAttributes, error) {
	query := `SELECT * from user_attributes WHERE user_id=$1`
	rows, err := d.db.Queryx(query, id)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var userAttrs UserAttributes
		err := rows.StructScan(&userAttrs)
		return &userAttrs, err
	}

	return nil, ErrUserAttributesNotFound
}

// SetUserAttributes updates the user attributes for the given user.
func (d *Datastore) SetUserAttributes(attributes *UserAttributes) error {
	tx, err := d.db.Beginx()
	if err != nil {
		return err
	}
	defer tx.Rollback()

	cols := []string{}
	params := []string{}

	if attributes.TourSeen != nil {
		cols = append(cols, "tour_seen")
		params = append(params, ":tour_seen")
	}

	query := `UPDATE user_attributes SET (%s) = (%s) WHERE user_id = :user_id`
	if len(cols) == 1 {
		query = `UPDATE user_attributes SET %s = %s WHERE user_id = :user_id`
	}
	_, err = d.db.NamedExec(fmt.Sprintf(query, strings.Join(cols, ","), strings.Join(params, ",")), attributes)

	if err != nil {
		return err
	}

	err = tx.Commit()
	if err != nil {
		return err
	}

	return nil
}

// createUserAttributesUsingTxn creates default user attributes for the given user.
func (d *Datastore) createUserAttributesUsingTxn(tx *sqlx.Tx, id uuid.UUID) error {
	query := `INSERT INTO user_attributes (user_id) VALUES ($1)`
	_, err := tx.Exec(query, id)

	if err != nil {
		return err
	}

	return nil
}
