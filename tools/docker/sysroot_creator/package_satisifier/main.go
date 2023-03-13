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

package main

import (
	"fmt"
	"os"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("pkgdb", "", "Debian distribution extracted Packages.xz file.")
	pflag.StringSlice("specs", []string{}, "List of yaml files specifying packages to include/exclude in the sysroot")
}

func main() {
	pflag.Parse()
	viper.BindPFlags(pflag.CommandLine)
	log.SetOutput(os.Stderr)

	packageDatabaseFile := viper.GetString("pkgdb")
	if packageDatabaseFile == "" {
		log.Fatal("must specify pkgdb")
	}
	specs := viper.GetStringSlice("specs")
	if len(specs) == 0 {
		log.Fatal("must specify at least one spec")
	}

	db, err := parsePackageDatabase(packageDatabaseFile)
	if err != nil {
		log.WithError(err).Fatal("failed to parse package database")
	}

	combinedSpec, err := parseAndCombineSpecs(specs)
	if err != nil {
		log.WithError(err).Fatal("failed to parse specs")
	}
	filteredSpec := combinedSpec.removeIncludesFromExclude()

	dependencySatisfier := newDepSatisfier(db, filteredSpec)

	debs, err := dependencySatisfier.listRequiredDebs()
	if err != nil {
		log.WithError(err).Fatal("failed to find all required packages")
	}
	for _, filename := range debs {
		fmt.Println(filename)
	}
}
