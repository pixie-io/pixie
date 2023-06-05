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
	"crypto/rand"
	"errors"
	"fmt"
	"strings"

	"github.com/gofrs/uuid"
	"github.com/jackc/pgx"
	"github.com/jmoiron/sqlx"
)

const (
	// See https://www.postgresql.org/docs/current/errcodes-appendix.html
	// Code for `unique_violation`
	uniqueViolation = "23505"

	// SaltLength is the length of the salt used when encrypting the invite signing key.
	SaltLength = 10
)

// TODO(zasgar): Move these to models ?

// UserInfo tracks information about a specific end-user.
type UserInfo struct {
	ID               uuid.UUID  `db:"id"`
	OrgID            *uuid.UUID `db:"org_id"`
	FirstName        string     `db:"first_name"`
	LastName         string     `db:"last_name"`
	Email            string     `db:"email"`
	ProfilePicture   *string    `db:"profile_picture"`
	IsApproved       bool       `db:"is_approved"`
	IdentityProvider string     `db:"identity_provider"`
	AuthProviderID   string     `db:"auth_provider_id"`
}

// OrgInfo tracks information about an organization.
type OrgInfo struct {
	ID              uuid.UUID `db:"id"`
	OrgName         string    `db:"org_name"`
	DomainName      *string   `db:"domain_name"`
	EnableApprovals bool      `db:"enable_approvals"`
}

// GetDomainName is a helper to nil check the DomainName column value and convert
// NULLs into empty strings for ease of use.
func (o *OrgInfo) GetDomainName() string {
	if o.DomainName == nil {
		return ""
	}
	return *o.DomainName
}

// Datastore is a postgres backed storage for entities.
type Datastore struct {
	db    *sqlx.DB
	dbKey string
}

// NewDatastore creates a Datastore.
func NewDatastore(db *sqlx.DB, dbKey string) *Datastore {
	return &Datastore{db: db, dbKey: dbKey}
}

var (
	// ErrUserNotFound is used when a user is not found when looking up by a filter condition.
	ErrUserNotFound = errors.New("user not found")
	// ErrOrgNotFound is used when the org is not found when looking up by a filter condition.
	ErrOrgNotFound = errors.New("org not found")
	// ErrNoInviteKey is used when the org doesn't have a invite key set.
	ErrNoInviteKey = errors.New("org has no invite signing key")
	// ErrUserAttributesNotFound is used when no attributes can be found for the given user.
	ErrUserAttributesNotFound = errors.New("user attributes not found")
	// ErrUserSettingsNotFound is used when no settings can be found for the given user.
	ErrUserSettingsNotFound = errors.New("user settings not found")
	// ErrDuplicateOrgName is used when the given org name is already in use.
	ErrDuplicateOrgName = errors.New("cannot create org (name already in use)")
	// ErrDuplicateUser is used when the user creation violates unique constraints for auth_provider_id or email.
	ErrDuplicateUser = errors.New("cannot create duplicate user")
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
	query := `SELECT id, org_id, first_name, last_name, email, profile_picture, is_approved, identity_provider, auth_provider_id FROM users WHERE id=$1`
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
	orgInfo.ID = u
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
	userInfo.OrgID = &orgID
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
	userInfo.OrgID = &orgID

	return orgID, userID, txn.Commit()
}

// GetOrg gets org information by ID.
func (d *Datastore) GetOrg(id uuid.UUID) (*OrgInfo, error) {
	query := `SELECT id, org_name, domain_name, enable_approvals FROM orgs WHERE id=$1`
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
	query := `SELECT id, org_name, domain_name, enable_approvals FROM orgs`
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

// GetOrgByName gets org information by domain.
func (d *Datastore) GetOrgByName(name string) (*OrgInfo, error) {
	query := `SELECT id, org_name, domain_name, enable_approvals FROM orgs WHERE org_name=$1`
	rows, err := d.db.Queryx(query, name)
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

// GetOrgByDomain gets org information by domain.
func (d *Datastore) GetOrgByDomain(domainName string) (*OrgInfo, error) {
	query := `SELECT id, org_name, domain_name, enable_approvals FROM orgs WHERE domain_name=$1`
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

// GetInviteSigningKey gets the invite signing key for the given orgID.
func (d *Datastore) GetInviteSigningKey(id uuid.UUID) (string, error) {
	query := `SELECT PGP_SYM_DECRYPT(invite_signing_key::bytea, $2) FROM orgs WHERE id=$1`
	rows, err := d.db.Queryx(query, id, d.dbKey)
	if err != nil {
		return "", ErrNoInviteKey
	}
	defer rows.Close()

	if rows.Next() {
		var saltedKey string
		err := rows.Scan(&saltedKey)
		if err != nil {
			return "", ErrNoInviteKey
		}
		if len(saltedKey) > SaltLength {
			inviteKey := saltedKey[SaltLength:]
			return inviteKey, nil
		}
	}

	return "", ErrNoInviteKey
}

// CreateInviteSigningKey creates an invite signing key for the given orgID.
func (d *Datastore) CreateInviteSigningKey(id uuid.UUID) (string, error) {
	inviteKey := make([]byte, 64)
	_, err := rand.Read(inviteKey)
	if err != nil {
		return "", errors.New("could not generate signing key")
	}

	// Add a salt to the signing key.
	salt := make([]byte, SaltLength/2)
	_, err = rand.Read(salt)
	if err != nil {
		return "", errors.New("could not create salt")
	}
	saltedKey := fmt.Sprintf("%x%x", salt, inviteKey)

	query := `UPDATE orgs SET invite_signing_key = PGP_SYM_ENCRYPT($2, $3) WHERE id = $1`
	_, err = d.db.Exec(query, id, saltedKey, d.dbKey)
	if err != nil {
		return "", err
	}

	return fmt.Sprintf("%x", inviteKey), nil
}

// GetUserByEmail gets user info by email.
func (d *Datastore) GetUserByEmail(email string) (*UserInfo, error) {
	query := `SELECT id, org_id, first_name, last_name, email, profile_picture, is_approved, identity_provider, auth_provider_id FROM users WHERE email=$1`
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
	query := `SELECT id, org_id, first_name, last_name, email, profile_picture, is_approved, identity_provider, auth_provider_id FROM users WHERE auth_provider_id=$1`
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

// DeleteUser deletes a user, but not the org.
func (d *Datastore) DeleteUser(userID uuid.UUID) error {
	txn, err := d.db.Beginx()
	if err != nil {
		return err
	}
	defer txn.Rollback()

	deleteUserSettingsQuery := `DELETE FROM user_settings WHERE user_id=$1`
	deleteUserAttributesQuery := `DELETE FROM user_attributes WHERE user_id=$1`
	deleteUserQuery := `DELETE FROM users WHERE id=$1`

	_, err = txn.Exec(deleteUserSettingsQuery, userID)
	if err != nil {
		return err
	}
	_, err = txn.Exec(deleteUserAttributesQuery, userID)
	if err != nil {
		return err
	}
	_, err = txn.Exec(deleteUserQuery, userID)
	if err != nil {
		return err
	}
	return txn.Commit()
}

// DeleteOrgAndUsers deletes the org and users with a given org ID.
func (d *Datastore) DeleteOrgAndUsers(orgID uuid.UUID) error {
	txn, err := d.db.Beginx()
	if err != nil {
		return err
	}
	defer txn.Rollback()

	deleteUserSettingsQuery := `DELETE from user_settings USING users where users.id=user_settings.user_id and users.org_id=$1`
	deleteUserAttributesQuery := `DELETE from user_attributes USING users where users.id=user_attributes.user_id and users.org_id=$1`
	deleteUsersQuery := `DELETE FROM users WHERE org_id=$1`
	deleteOrgQuery := `DELETE FROM orgs WHERE id=$1`
	_, err = txn.Exec(deleteUserSettingsQuery, orgID)
	if err != nil {
		return err
	}
	_, err = txn.Exec(deleteUserAttributesQuery, orgID)
	if err != nil {
		return err
	}
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
	query := `INSERT INTO users (org_id, first_name, last_name, email, is_approved, identity_provider, auth_provider_id) VALUES (:org_id, :first_name, :last_name, :email, :is_approved, :identity_provider, :auth_provider_id) RETURNING id`
	rows, err := txn.NamedQuery(query, userInfo)
	if err != nil {
		return uuid.Nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var id uuid.UUID
		if err := rows.Scan(&id); err != nil {
			return uuid.Nil, err
		}
		return id, nil
	}
	err = rows.Err()
	switch e := err.(type) {
	case pgx.PgError:
		if e.Code == uniqueViolation {
			return uuid.Nil, ErrDuplicateUser
		}
	}
	return uuid.Nil, err
}

func (d *Datastore) createOrgUsingTxn(txn *sqlx.Tx, orgInfo *OrgInfo) (uuid.UUID, error) {
	query := `INSERT INTO orgs (org_name, domain_name) VALUES (:org_name, :domain_name) RETURNING id`
	rows, err := txn.NamedQuery(query, orgInfo)
	if err != nil {
		return uuid.Nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var id uuid.UUID
		if err := rows.Scan(&id); err != nil {
			return uuid.Nil, err
		}
		return id, nil
	}
	err = rows.Err()
	switch e := err.(type) {
	case pgx.PgError:
		if e.Code == uniqueViolation {
			return uuid.Nil, ErrDuplicateOrgName
		}
	}
	return uuid.Nil, err
}

// GetUsersInOrg gets all users in the given org.
func (d *Datastore) GetUsersInOrg(orgID uuid.UUID) ([]*UserInfo, error) {
	query := `SELECT id, org_id, first_name, last_name, email, profile_picture, is_approved, identity_provider, auth_provider_id FROM users WHERE org_id=$1 order by created_at desc`
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

// NumUsersInOrg gets the count of users in the given org.
func (d *Datastore) NumUsersInOrg(orgID uuid.UUID) (int, error) {
	query := `SELECT count(1) FROM users WHERE org_id=$1`
	rows, err := d.db.Queryx(query, orgID)
	if err != nil {
		return 0, err
	}
	defer rows.Close()

	if rows.Next() {
		var count int
		err := rows.Scan(&count)
		if err != nil {
			return 0, err
		}
		return count, nil
	}
	return 0, errors.New("failed to count number of users in org")
}

// UpdateUser updates the user in the database.
func (d *Datastore) UpdateUser(userInfo *UserInfo) error {
	query := `UPDATE users SET profile_picture = :profile_picture, is_approved = :is_approved, org_id = :org_id WHERE id = :id`
	_, err := d.db.NamedExec(query, userInfo)
	return err
}

// UpdateOrg updates the org in the database.
func (d *Datastore) UpdateOrg(orgInfo *OrgInfo) error {
	query := `UPDATE orgs SET enable_approvals = :enable_approvals, domain_name = :domain_name WHERE id = :id`
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

// IDEConfig is an org-level configuration which defines the IDE paths which can be used to navigate to
// a given symbol.
type IDEConfig struct {
	Name string `db:"ide_name"`
	Path string `db:"path"`
}

// AddIDEConfig adds the IDE config to the org.
func (d *Datastore) AddIDEConfig(orgID uuid.UUID, config *IDEConfig) error {
	query := `INSERT INTO org_ide_configs (org_id, ide_name, path) VALUES ($1, $2, $3)`
	_, err := d.db.Exec(query, orgID, config.Name, config.Path)
	if err != nil {
		return err
	}
	return nil
}

// DeleteIDEConfig deletes the IDE config from the org.
func (d *Datastore) DeleteIDEConfig(orgID uuid.UUID, name string) error {
	query := `DELETE FROM org_ide_configs WHERE org_id=$1 AND ide_name=$2`
	_, err := d.db.Exec(query, orgID, name)
	if err != nil {
		return err
	}
	return nil
}

// GetIDEConfigs gets all IDE configs for the org.
func (d *Datastore) GetIDEConfigs(orgID uuid.UUID) ([]*IDEConfig, error) {
	query := `SELECT ide_name, path from org_ide_configs WHERE org_id=$1`
	rows, err := d.db.Queryx(query, orgID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	configs := make([]*IDEConfig, 0)
	for rows.Next() {
		var ideConf IDEConfig
		err := rows.StructScan(&ideConf)
		if err != nil {
			return nil, err
		}
		configs = append(configs, &ideConf)
	}
	return configs, nil
}

// GetIDEConfig gets the IDE config for the IDE with the given name.
func (d *Datastore) GetIDEConfig(orgID uuid.UUID, name string) (*IDEConfig, error) {
	query := `SELECT ide_name, path from org_ide_configs WHERE org_id=$1 AND ide_name=$2`
	rows, err := d.db.Queryx(query, orgID, name)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	if rows.Next() {
		var ideConf IDEConfig
		err := rows.StructScan(&ideConf)
		return &ideConf, err
	}
	return nil, errors.New("failed to get IDE config for IDE with given name")
}
