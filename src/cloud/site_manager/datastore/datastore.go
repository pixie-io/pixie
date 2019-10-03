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
	OrgID    uuid.UUID `db:"org_id"`
	SiteName string    `db:"site_name"`
}

// NewDatastore creates a new site manager Datastore.
func NewDatastore(db *sqlx.DB) (*Datastore, error) {
	return &Datastore{db: db}, nil
}

// CheckAvailability checks the database to see if the site is still available.
func (d *Datastore) CheckAvailability(siteName string) (bool, error) {
	var exists bool

	query := `SELECT exists(SELECT site_name from sites WHERE site_name=$1)`
	err := d.db.QueryRow(query, siteName).Scan(&exists)
	if err != nil && err != sql.ErrNoRows {
		return false, err
	}

	return !exists, nil
}

// RegisterSite assigs ownership of the site to a particular org name. Doing this on a site that already exists is an error.
func (d *Datastore) RegisterSite(orgID uuid.UUID, siteName string) error {
	query := `INSERT INTO sites (org_id, site_name) VALUES ($1, $2)`
	_, err := d.db.Exec(query, orgID, siteName)
	return err
}

// GetSiteForOrg gets the site for a particular org specified by ID.
func (d *Datastore) GetSiteForOrg(orgID uuid.UUID) (*SiteInfo, error) {
	query := `SELECT org_id, site_name from sites WHERE org_id=$1`
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

// GetSiteByName gets the site based on the site name.
func (d *Datastore) GetSiteByName(siteName string) (*SiteInfo, error) {
	query := `SELECT org_id, site_name from sites WHERE site_name=$1`
	siteInfo := &SiteInfo{}
	row := d.db.QueryRowx(query, siteName)
	switch err := row.StructScan(siteInfo); err {
	case sql.ErrNoRows:
		return nil, nil
	case nil:
		return siteInfo, nil
	default:
		return nil, err
	}
}
