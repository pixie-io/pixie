package datastore

import (
	"database/sql"

	"github.com/jmoiron/sqlx"
	uuid "github.com/satori/go.uuid"
)

// Datastore implementation for sitemanager using a PGSQL backend.
type Datastore struct {
	db *sqlx.DB
}

// SiteInfo describes a specific site.
type SiteInfo struct {
	OrgID      uuid.UUID `db:"org_id"`
	DomainName string    `db:"domain_name"`
}

// NewDatastore creates a new site manager Datastore.
func NewDatastore(db *sqlx.DB) (*Datastore, error) {
	return &Datastore{db: db}, nil
}

// CheckAvailability checks the database to see if the site is still available.
func (d *Datastore) CheckAvailability(domainName string) (bool, error) {
	var exists bool

	query := `SELECT exists(SELECT domain_name from sites WHERE domain_name=$1)`
	err := d.db.QueryRow(query, domainName).Scan(&exists)
	if err != nil && err != sql.ErrNoRows {
		return false, err
	}

	return !exists, nil
}

// RegisterSite assigs ownership of the site to a particular org name. Doing this on a site that already exists is an error.
func (d *Datastore) RegisterSite(orgID uuid.UUID, domainName string) error {
	query := `INSERT INTO sites (org_id, domain_name) VALUES ($1, $2)`
	_, err := d.db.Exec(query, orgID, domainName)
	return err
}

// GetSiteForOrg gets the site for a particular org specified by ID.
func (d *Datastore) GetSiteForOrg(orgID uuid.UUID) (*SiteInfo, error) {
	query := `SELECT * from sites WHERE org_id=$1`
	siteInfo := &SiteInfo{}
	row := d.db.QueryRowx(query, orgID)
	switch err := row.StructScan(siteInfo); err {
	case sql.ErrNoRows:
		return nil, nil
	case nil:
		return siteInfo, nil
	default:
		return nil, err
	}
}

// GetSiteByDomain gets the site based on the domain name.
func (d *Datastore) GetSiteByDomain(domainName string) (*SiteInfo, error) {
	query := `SELECT * from sites WHERE domain_name=$1`
	siteInfo := &SiteInfo{}
	row := d.db.QueryRowx(query, domainName)
	switch err := row.StructScan(siteInfo); err {
	case sql.ErrNoRows:
		return nil, nil
	case nil:
		return siteInfo, nil
	default:
		return nil, err
	}
}
