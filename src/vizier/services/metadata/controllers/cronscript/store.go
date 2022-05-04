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

package cronscript

import (
	"fmt"
	"path"
	"strconv"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/utils/datastore"
)

const (
	cronScriptPrefix    = "/cronScript/"
	scriptResultsPrefix = "/cronScriptResults"
	// The maximum results we store per CronScript. Note if you change this, you must ensure this value is than 10000
	// otherwise the string formatter will fail and you'll run into issues related to the prefix.
	maxResultsPerCronScript = 10
)

// Datastore implements the CronScriptStore interface on a given Datastore.
type Datastore struct {
	ds datastore.MultiGetterSetterDeleterCloser
}

// NewDatastore wraps the datastore in a cronScriptStore.
func NewDatastore(ds datastore.MultiGetterSetterDeleterCloser) *Datastore {
	return &Datastore{ds: ds}
}

func getCronScriptKey(scriptID uuid.UUID) string {
	return path.Join(cronScriptPrefix, scriptID.String())
}

// The Schema for ScriptResults:
// root:              /cronScriptResults/<id>
// results index:     /cronScriptResults/<id>/index
// all results:       /cronScriptResults/<id>/results
// specific results:  /cronScriptResults/<id>/results/<index>
//
// We implement the results as a ringbuffer where the current index is also stored in the db.
// After each write, we set index := (index + 1 mod maxResultsPerConScript).
// Once we exceed maxResultsPerConScript, we write over the old data at index 0 and so on.
func getCronScriptResultsKey(scriptID uuid.UUID) string {
	return path.Join(scriptResultsPrefix, scriptID.String(), "results")
}

// We store the results as a subpath under the scriptID for easy access.
func getCronScriptSpecificResultKey(scriptID uuid.UUID, index int64) string {
	return path.Join(getCronScriptResultsKey(scriptID), fmt.Sprintf("%04d", index))
}

// We reserve a separate subpath for the index itself.
func getCronScriptResultsIndexKey(scriptID uuid.UUID) string {
	return path.Join(scriptResultsPrefix, scriptID.String(), "index")
}

// GetCronScripts fetches all scripts in the cron script store.
func (t *Datastore) GetCronScripts() ([]*cvmsgspb.CronScript, error) {
	_, vals, err := t.ds.GetWithPrefix(cronScriptPrefix)
	if err != nil {
		return nil, err
	}

	scripts := make([]*cvmsgspb.CronScript, len(vals))
	for i, val := range vals {
		pb := &cvmsgspb.CronScript{}
		err := proto.Unmarshal(val, pb)
		if err != nil {
			continue
		}
		scripts[i] = pb
	}
	return scripts, nil
}

// UpsertCronScript updates or adds a cron script to the store, based on ID.
func (t *Datastore) UpsertCronScript(script *cvmsgspb.CronScript) error {
	val, err := script.Marshal()
	if err != nil {
		return err
	}

	sID := utils.UUIDFromProtoOrNil(script.ID)

	return t.ds.Set(getCronScriptKey(sID), string(val))
}

// DeleteCronScript deletes a cron script from the store by ID.
func (t *Datastore) DeleteCronScript(id uuid.UUID) error {
	err := t.ds.DeleteWithPrefix(getCronScriptKey(id))
	if err != nil {
		return err
	}
	return t.ds.DeleteWithPrefix(getCronScriptResultsKey(id))
}

// SetCronScripts sets the list of all cron scripts to match the given set of scripts.
func (t *Datastore) SetCronScripts(scripts []*cvmsgspb.CronScript) error {
	existingScripts, err := t.GetCronScripts()
	if err != nil {
		return err
	}
	removedScriptIDs := make(map[uuid.UUID]struct{})
	for _, es := range existingScripts {
		if es == nil {
			continue
		}
		removedScriptIDs[utils.UUIDFromProtoOrNil(es.ID)] = struct{}{}
	}

	// Update with new scripts.
	// We may want to make this a MultiSetter at some point.
	var lastError error
	for _, s := range scripts {
		err = t.UpsertCronScript(s)
		if err != nil {
			lastError = err
			log.WithError(err).Error("Failed to add cron script")
		}

		// A script that appears here was not removed.
		// It is safe to delete elements that don't exist in the original map.
		delete(removedScriptIDs, utils.UUIDFromProtoOrNil(s.ID))
	}

	if lastError != nil {
		return lastError
	}

	// Delete all results for removed scripts
	for k := range removedScriptIDs {
		err := t.DeleteCronScript(k)
		if err != nil {
			lastError = err
		}
	}

	return lastError
}

// GetCronScriptResults returns the results of past runs of a specific CronScript.
func (t *Datastore) GetCronScriptResults(id uuid.UUID) ([]*storepb.CronScriptResult, error) {
	_, vals, err := t.ds.GetWithPrefix(getCronScriptResultsKey(id))
	if err != nil {
		return nil, err
	}
	results := make([]*storepb.CronScriptResult, len(vals))
	for i, val := range vals {
		pb := &storepb.CronScriptResult{}
		err := proto.Unmarshal(val, pb)
		if err != nil {
			continue
		}
		results[i] = pb
	}
	return results, nil
}

func (t *Datastore) getCronScriptResultsIndex(scriptID uuid.UUID) (int64, error) {
	val, err := t.ds.Get(getCronScriptResultsIndexKey(scriptID))
	if err != nil {
		return 0, err
	}
	if len(val) == 0 {
		return 0, nil
	}

	i, err := strconv.Atoi(string(val))
	if err != nil {
		return 0, nil
	}

	return int64(i), nil
}

// RecordCronScriptResult saves the data for the latest result. If it exceeds the maximum number for that result, the call will delete the oldest entry.
func (t *Datastore) RecordCronScriptResult(result *storepb.CronScriptResult) error {
	scriptID := utils.UUIDFromProtoOrNil(result.ScriptID)
	idx, err := t.getCronScriptResultsIndex(scriptID)
	if err != nil {
		return err
	}
	val, err := result.Marshal()
	if err != nil {
		return err
	}
	err = t.ds.Set(getCronScriptSpecificResultKey(scriptID, idx), string(val))
	if err != nil {
		return err
	}
	// Increment the index.
	return t.ds.Set(getCronScriptResultsIndexKey(scriptID), fmt.Sprint((idx+1)%maxResultsPerCronScript))
}

// GetAllCronScriptResults returns all of the stored execution results for all scripts.
func (t *Datastore) GetAllCronScriptResults() ([]*storepb.CronScriptResult, error) {
	keys, vals, err := t.ds.GetWithPrefix(scriptResultsPrefix)
	if err != nil {
		return nil, err
	}
	var results []*storepb.CronScriptResult
	for i, k := range keys {
		if path.Base(k) == "index" {
			continue
		}
		newResult := &storepb.CronScriptResult{}
		err := proto.Unmarshal(vals[i], newResult)
		if err != nil {
			return nil, err
		}
		results = append(results, newResult)
	}
	return results, nil
}
