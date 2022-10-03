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

package script

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"sort"
	"strings"
	"sync"

	log "github.com/sirupsen/logrus"
)

// BundleManager reads a script bundle.
type BundleManager struct {
	scripts map[string]*pixieScript
}

func pixieScriptToExecutableScript(scriptName string, script *pixieScript) (*ExecutableScript, error) {
	vs, err := ParseVisSpec(script.Vis)
	if err != nil {
		return nil, err
	}
	return &ExecutableScript{
		ScriptName:   scriptName,
		ShortDoc:     script.ShortDoc,
		LongDoc:      script.LongDoc,
		Vis:          vs,
		ScriptString: script.Pxl,
		OrgID:        script.OrgID,
		Hidden:       script.Hidden,
	}, nil
}

func isValidURL(toTest string) bool {
	_, err := url.ParseRequestURI(toTest)
	if err != nil {
		return false
	}

	u, err := url.Parse(toTest)
	if err != nil || u.Scheme == "" || u.Host == "" {
		return false
	}

	return true
}

// NewBundleManagerWithOrg reads the json bundle and initializes the bundle reader for a specific org.
func NewBundleManagerWithOrg(bundleFiles []string, orgID, orgName string) (*BundleManager, error) {
	var wg sync.WaitGroup
	wg.Add(len(bundleFiles))
	bundles := make([]*bundle, len(bundleFiles))

	readBundle := func(bundleFile string, index int) {
		defer wg.Done()
		var r io.Reader
		if isValidURL(bundleFile) {
			resp, err := http.Get(bundleFile)
			if err != nil {
				log.WithError(err).Error("Error checking bundle file URL")
				return
			}
			defer resp.Body.Close()
			r = resp.Body
		} else {
			f, err := os.Open(bundleFile)
			if err != nil {
				log.WithError(err).Error("Error reading bundle file")
				return
			}
			defer f.Close()
			r = f
		}

		var b bundle
		err := json.NewDecoder(r).Decode(&b)
		if err != nil {
			log.WithError(err).Error("Error decoding bundle file")
			return
		}

		bundles[index] = &b
	}

	for i, bundle := range bundleFiles {
		go readBundle(bundle, i)
	}
	wg.Wait()

	filtered := make(map[string]*pixieScript)
	// Filter scripts by org.
	for _, b := range bundles {
		if b != nil {
			for k, script := range b.Scripts {
				if len(script.OrgID) == 0 || script.OrgID == orgID {
					prettyID := k
					if strings.HasPrefix(k, "org_id/") {
						if orgName == "" {
							continue
						}
						splits := strings.SplitN(k, "/", 3)
						if len(splits) < 3 {
							continue
						}
						if splits[1] != orgID {
							continue
						}
						prettyID = fmt.Sprintf("%s/%s", orgName, splits[2])
					}
					filtered[prettyID] = script
				}
			}
		}
	}

	return &BundleManager{
		scripts: filtered,
	}, nil
}

// GetScripts returns metadata about available scripts.
func (b BundleManager) GetScripts() []*ExecutableScript {
	s := make([]*ExecutableScript, 0)
	i := 0
	for k, val := range b.scripts {
		pixieScript, err := pixieScriptToExecutableScript(k, val)
		if err != nil {
			log.WithError(err).Error("Failed to parse script, skipping...")
			continue
		}
		s = append(s, pixieScript)
		i++
	}

	// Sort scripts to make sure .
	sort.Slice(s, func(i, j int) bool {
		return s[i].ScriptName < s[j].ScriptName
	})

	return s
}

// GetOrderedScripts returns metadata about available scripts ordered by the name of the script.
func (b BundleManager) GetOrderedScripts() []*ExecutableScript {
	s := b.GetScripts()
	sort.Slice(s, func(i, j int) bool {
		return s[i].ScriptName < s[j].ScriptName
	})
	return s
}

// GetScript returns the script by name.
func (b BundleManager) GetScript(scriptName string) (*ExecutableScript, error) {
	script, ok := b.scripts[scriptName]
	if !ok {
		return nil, ErrScriptNotFound
	}
	return pixieScriptToExecutableScript(scriptName, script)
}

// MustGetScript is GetScript with fatal on error.
func (b BundleManager) MustGetScript(scriptName string) *ExecutableScript {
	es, err := b.GetScript(scriptName)
	if err != nil {
		log.WithError(err).Fatal("Failed to get script")
	}
	return es
}

// AddScript adds the specified script to the bundle manager.
func (b *BundleManager) AddScript(script *ExecutableScript) error {
	n := script.ScriptName

	_, has := b.scripts[n]
	if has {
		return errors.New("script with same name already exists")
	}
	p := &pixieScript{
		Pxl:      script.ScriptString,
		ShortDoc: script.ShortDoc,
		LongDoc:  script.LongDoc,
	}
	b.scripts[n] = p
	return nil
}
