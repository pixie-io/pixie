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
	"context"
	"errors"
	"sync"

	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

// Server is an implementation of the cronscriptstore service.
type Server struct {
	ds *Datastore

	done chan struct{}
	once sync.Once
}

// New creates a new server.
func New(ds *Datastore) *Server {
	return &Server{
		ds:   ds,
		done: make(chan struct{}),
	}
}

// Stop performs any necessary cleanup before shutdown.
func (s *Server) Stop() {
	s.once.Do(func() {
		close(s.done)
	})
}

// GetScripts fetches all scripts in the cron script store.
func (s *Server) GetScripts(ctx context.Context, req *metadatapb.GetScriptsRequest) (*metadatapb.GetScriptsResponse, error) {
	return nil, errors.New("Not yet implemented")
}

// AddOrUpdateScript updates or adds a cron script to the store, based on ID.
func (s *Server) AddOrUpdateScript(ctx context.Context, req *metadatapb.AddOrUpdateScriptRequest) (*metadatapb.AddOrUpdateScriptResponse, error) {
	return nil, errors.New("Not yet implemented")
}

// DeleteScript deletes a cron script from the store by ID.
func (s *Server) DeleteScript(ctx context.Context, req *metadatapb.DeleteScriptRequest) (*metadatapb.DeleteScriptResponse, error) {
	return nil, errors.New("Not yet implemented")
}

// SetScripts sets the list of all cron scripts to match the given set of scripts.
func (s *Server) SetScripts(ctx context.Context, req *metadatapb.SetScriptsRequest) (*metadatapb.SetScriptsResponse, error) {
	return nil, errors.New("Not yet implemented")
}
