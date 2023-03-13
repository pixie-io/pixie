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
	"bufio"
	"fmt"
	"os"
	"regexp"
	"strings"
)

var simpleDepRegex *regexp.Regexp

func init() {
	var err error
	simpleDepRegex, err = regexp.Compile(`([^ :]*)([:][^ ]+)?(.*)?`)
	if err != nil {
		panic(err)
	}
}

type dependency struct {
	// anyOf stores each package that could satisfy this dependency. i.e. an OR dependency on each of the packages listed.
	anyOf []string
}

func (d *dependency) String() string {
	return strings.Join(d.anyOf, " | ")
}

type pkg struct {
	name     string
	filename string
	depends  []*dependency
	// list of virtual packages provided by this package.
	provides []string
}

type database struct {
	packages         map[string]*pkg
	virtualProviders map[string][]string
}

func (p *pkg) String() string {
	b := &strings.Builder{}
	_, _ = b.WriteString("Package: " + p.name + "\n")
	_, _ = b.WriteString("Depends:\n")
	for _, dep := range p.depends {
		_, _ = b.WriteString(dep.String())
		_, _ = b.WriteString("\n")
	}
	_, _ = b.WriteString("Provides:\n")
	for _, name := range p.provides {
		_, _ = b.WriteString(name + "\n")
	}
	return b.String()
}

func newPkg(name string) *pkg {
	return &pkg{
		name:     name,
		depends:  make([]*dependency, 0),
		provides: make([]string, 0),
	}
}

func parseProvides(str string, curPkg *pkg) error {
	provides := strings.Split(str, ", ")
	for _, depStr := range provides {
		name, err := parseSimpleDep(depStr)
		if err != nil {
			return err
		}
		curPkg.provides = append(curPkg.provides, name)
	}
	return nil
}

func parseSimpleDep(depStr string) (string, error) {
	matches := simpleDepRegex.FindStringSubmatch(depStr)
	if len(matches) < 2 {
		return "", fmt.Errorf("no regex match for dependency: %s", depStr)
	}
	return matches[1], nil
}

func parseDep(str string) (*dependency, error) {
	depStrings := strings.Split(str, " | ")
	dep := &dependency{
		anyOf: make([]string, 0),
	}
	for _, depStr := range depStrings {
		depPkg, err := parseSimpleDep(depStr)
		if err != nil {
			return nil, err
		}
		dep.anyOf = append(dep.anyOf, depPkg)
	}
	return dep, nil
}

func parseDepends(str string, curPkg *pkg) error {
	depends := strings.Split(str, ", ")
	for _, depStr := range depends {
		dep, err := parseDep(depStr)
		if err != nil {
			return err
		}
		curPkg.depends = append(curPkg.depends, dep)
	}
	return nil
}

func parsePackageDatabase(path string) (*database, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}

	db := &database{
		packages:         make(map[string]*pkg),
		virtualProviders: make(map[string][]string),
	}
	var curPkg *pkg
	scanner := bufio.NewScanner(f)
	// Increase maximum token size of scanner.
	bufSize := 512 * 1024
	buf := make([]byte, 0, bufSize)
	scanner.Buffer(buf, bufSize)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "Package:") {
			if curPkg != nil {
				db.packages[curPkg.name] = curPkg
			}
			curPkg = newPkg(strings.TrimPrefix(line, "Package: "))
		}
		if strings.HasPrefix(line, "Provides: ") {
			if err := parseProvides(strings.TrimPrefix(line, "Provides: "), curPkg); err != nil {
				return nil, err
			}
			for _, virtualPkg := range curPkg.provides {
				if _, ok := db.virtualProviders[virtualPkg]; !ok {
					db.virtualProviders[virtualPkg] = make([]string, 0)
				}
				db.virtualProviders[virtualPkg] = append(db.virtualProviders[virtualPkg], curPkg.name)
			}
		}
		if strings.HasPrefix(line, "Pre-Depends: ") {
			if err := parseDepends(strings.TrimPrefix(line, "Pre-Depends: "), curPkg); err != nil {
				return nil, err
			}
		}
		if strings.HasPrefix(line, "Depends: ") {
			if err := parseDepends(strings.TrimPrefix(line, "Depends: "), curPkg); err != nil {
				return nil, err
			}
		}
		if strings.HasPrefix(line, "Filename: ") {
			curPkg.filename = strings.TrimPrefix(line, "Filename: ")
		}
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	if curPkg != nil {
		db.packages[curPkg.name] = curPkg
	}
	return db, nil
}
