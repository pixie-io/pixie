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
