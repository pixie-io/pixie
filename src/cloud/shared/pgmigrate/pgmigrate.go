// Package pgmigrate has utilities to help with postgres migrations.
package pgmigrate

import (
	"github.com/golang-migrate/migrate"
	"github.com/golang-migrate/migrate/database/postgres"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/jmoiron/sqlx"
)

// PerformMigrationsUsingBindata uses the passed in bindata assets to perform postgres DB migrations.
func PerformMigrationsUsingBindata(db *sqlx.DB, migrationTable string, assetSource *bindata.AssetSource) error {
	driver, err := postgres.WithInstance(db.DB, &postgres.Config{
		MigrationsTable: migrationTable,
	})
	if err != nil {
		return err
	}

	d, err := bindata.WithInstance(assetSource)
	if err != nil {
		return err
	}

	mg, err := migrate.NewWithInstance(
		"go-bindata",
		d, "postgres", driver)
	if err != nil {
		return err
	}

	if err = mg.Up(); err != nil && err != migrate.ErrNoChange {
		return err
	}
	return nil
}
