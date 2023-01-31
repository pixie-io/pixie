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
	"os"

	"gopkg.in/yaml.v3"
)

type spec struct {
	Includes []string `yaml:"include"`
	Excludes []string `yaml:"exclude"`
}

func (s *spec) removeIncludesFromExclude() *spec {
	newSpec := &spec{
		Includes: s.Includes[:],
		Excludes: make([]string, 0),
	}
	incMap := make(map[string]bool)
	for _, inc := range s.Includes {
		incMap[inc] = true
	}
	for _, exc := range s.Excludes {
		if incMap[exc] {
			continue
		}
		newSpec.Excludes = append(newSpec.Excludes, exc)
	}
	return newSpec
}

func specFromFile(path string) (*spec, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	dec := yaml.NewDecoder(f)
	s := &spec{}
	if err := dec.Decode(s); err != nil {
		return nil, err
	}
	return s, nil
}

func parseAndCombineSpecs(paths []string) (*spec, error) {
	allSpecs := make([]*spec, len(paths))
	for i, p := range paths {
		s, err := specFromFile(p)
		if err != nil {
			return nil, err
		}
		allSpecs[i] = s
	}

	return combine(allSpecs)
}

func combine(specs []*spec) (*spec, error) {
	combinedIncldues := make(map[string]bool)
	combinedExcludes := make(map[string]bool)

	for _, s := range specs {
		for _, inc := range s.Includes {
			combinedIncldues[inc] = true
		}
		for _, exc := range s.Excludes {
			combinedExcludes[exc] = true
		}
	}
	s := &spec{
		Includes: make([]string, 0, len(combinedIncldues)),
		Excludes: make([]string, 0, len(combinedExcludes)),
	}
	for inc := range combinedIncldues {
		s.Includes = append(s.Includes, inc)
	}
	for exc := range combinedExcludes {
		s.Excludes = append(s.Excludes, exc)
	}
	return s, nil
}
