package datastore

import (
	"errors"
	"fmt"
	"time"

	"github.com/jmoiron/sqlx"
	uuid "github.com/satori/go.uuid"
)

// TODO(zasgar): Move these to models ?

// UserInfo tracks information about a specific end-user.
type UserInfo struct {
	ID             uuid.UUID  `db:"id"`
	OrgID          uuid.UUID  `db:"org_id"`
	Username       string     `db:"username"`
	FirstName      string     `db:"first_name"`
	LastName       string     `db:"last_name"`
	Email          string     `db:"email"`
	ProfilePicture *string    `db:"profile_picture"`
	UpdatedAt      *time.Time `db:"updated_at"`
	CreatedAt      *time.Time `db:"created_at"`
}

// OrgInfo tracks information about an organization.
type OrgInfo struct {
	ID         uuid.UUID `db:"id"`
	OrgName    string    `db:"org_name"`
	DomainName string    `db:"domain_name"`
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
func (d *Datastore) CreateUserAndOrg(orgInfo *OrgInfo, userInfo *UserInfo) (orgID uuid.UUID, userID uuid.UUID, err error) {
	txn, err := d.db.Beginx()
	if err != nil {
		return
	}
	defer txn.Rollback()

	orgID, err = d.createOrgUsingTxn(txn, orgInfo)
	if err != nil {
		return
	}
	userInfo.OrgID = orgID
	userID, err = d.createUserUsingTxn(txn, userInfo)

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

// GetUserByEmail gets org information by domain.
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
	query := `INSERT INTO users (org_id, username, first_name, last_name, email) VALUES (:org_id, :username, :first_name, :last_name, :email) RETURNING id`
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

// UpdateUser updates the user in the database.
func (d *Datastore) UpdateUser(userInfo *UserInfo) error {
	query := `UPDATE users SET profile_picture = :profile_picture WHERE id = :id`
	row, err := d.db.NamedQuery(query, userInfo)
	if err != nil {
		return err
	}
	defer row.Close()

	return nil
}
