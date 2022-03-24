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
	"path"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/utils/datastore"
)

const (
	cronScriptPrefix = "/cronScript/"
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
	return t.ds.DeleteWithPrefix(getCronScriptKey(id))
}

// SetCronScripts sets the list of all cron scripts to match the given set of scripts.
func (t *Datastore) SetCronScripts(scripts []*cvmsgspb.CronScript) error {
	// Delete all existing scripts.
	err := t.ds.DeleteWithPrefix(cronScriptPrefix)
	if err != nil {
		return err
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
	}

	return lastError
}
