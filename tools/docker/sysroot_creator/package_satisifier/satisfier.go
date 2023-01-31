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
)

type depSatisfier struct {
	db       *database
	excludes map[string]bool
	includes []string

	visited map[string]bool
}

func newDepSatisfier(db *database, spec *spec) *depSatisfier {
	ds := &depSatisfier{
		db:       db,
		excludes: make(map[string]bool),
		includes: spec.Includes,

		visited: make(map[string]bool),
	}
	for _, exc := range spec.Excludes {
		ds.excludes[exc] = true
	}
	return ds
}

func (ds *depSatisfier) getPackage(name string) *pkg {
	p, ok := ds.db.packages[name]
	if !ok {
		return nil
	}
	return p
}

func (ds *depSatisfier) listRequiredDebs() ([]string, error) {
	// We do a breadth first search of the package dependency tree, to find all dependencies of the specified includes.
	q := make([]string, 0, len(ds.includes))
	q = append(q, ds.includes...)

	debs := make([]string, 0)

	for len(q) > 0 {
		name := q[0]
		q = q[1:]
		if ds.visited[name] {
			continue
		}
		ds.visited[name] = true
		// Don't follow dependencies of excluded packages.
		if ds.excludes[name] {
			continue
		}

		p := ds.getPackage(name)
		// If p is nil, then this package name refers to a virtual package.
		if p == nil {
			// If a virtual package is required. We need to choose a provider for it.
			provider, err := ds.findProvider(name)
			if err != nil {
				return nil, err
			}
			if provider != "" {
				q = append(q, provider)
			}
			continue
		}
		debs = append(debs, p.filename)

		// Mark all virtual packages this real package provides as visited.
		for _, provided := range p.provides {
			ds.visited[provided] = true
		}
		for _, d := range p.depends {
			dep := ds.resolveDep(d)
			if dep != "" {
				q = append(q, dep)
			}
		}
	}
	return debs, nil
}

func (ds *depSatisfier) resolveDep(d *dependency) string {
	if len(d.anyOf) == 0 {
		return ""
	}
	if len(d.anyOf) == 1 {
		return d.anyOf[0]
	}
	// At this point, the dependency contains an OR.
	// We first try to see if any of the OR dependencies have already been satisified.
	// Failing that we naively pick the first one of the OR.
	// We could keep the or condition around until the very end
	// then try to satisfy all the ORs minimally using a SAT solver or something of the like, but that's too much effor for now.

	for _, name := range d.anyOf {
		if ds.visited[name] {
			// Since its already visited we don't need to traverse it.
			return ""
		}
	}

	return d.anyOf[0]
}

func (ds *depSatisfier) findProvider(name string) (string, error) {
	providers, ok := ds.db.virtualProviders[name]
	if !ok {
		return "", fmt.Errorf("cannot find provider for virtual package '%s'", name)
	}
	for _, p := range providers {
		if ds.visited[p] {
			return "", nil
		}
	}
	// If we haven't already required one of the providers, then choose the first one in the list.
	return providers[0], nil
}
