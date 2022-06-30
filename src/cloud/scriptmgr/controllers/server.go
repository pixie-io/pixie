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

package controllers

import (
	"context"
	"fmt"
	"strings"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/jsonpb"
	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/cloud/scriptmgr/scriptmgrpb"
	"px.dev/pixie/src/utils"
)

type scriptModel struct {
	name        string
	desc        string
	pxl         string
	hasLiveView bool
}

type liveViewModel struct {
	name        string
	desc        string
	pxlContents string
	vis         *vispb.Vis
}

type scriptStore struct {
	Scripts   map[uuid.UUID]*scriptModel
	LiveViews map[uuid.UUID]*liveViewModel
}

// Server implements the GRPC Server for the scriptmgr service.
type Server struct {
	bundleBucket    string
	bundlePath      string
	sc              stiface.Client
	store           *scriptStore
	storeLastUpdate time.Time
	SeedUUID        uuid.UUID
}

// NewServer creates a new GRPC scriptmgr server.
func NewServer(bundleBucket string, bundlePath string, sc stiface.Client) *Server {
	s := &Server{
		bundleBucket: bundleBucket,
		bundlePath:   bundlePath,
		sc:           sc,
		store: &scriptStore{
			Scripts:   make(map[uuid.UUID]*scriptModel),
			LiveViews: make(map[uuid.UUID]*liveViewModel),
		},
		storeLastUpdate: time.Unix(0, 0),
		SeedUUID:        uuid.Must(uuid.NewV4()),
	}
	err := s.updateStore()
	if err != nil {
		log.WithError(err).
			WithField("bucket", s.bundleBucket).
			WithField("path", s.bundlePath).
			Error("Failed to update store using bundle.json from gcs.")
	}
	return s
}

func (s *Server) addLiveView(name string, bundleScript *pixieScript) error {
	id := uuid.NewV5(s.SeedUUID, name)

	var vis vispb.Vis
	err := jsonpb.UnmarshalString(bundleScript.Vis, &vis)
	if err != nil {
		return err
	}

	s.store.LiveViews[id] = &liveViewModel{
		name:        name,
		desc:        bundleScript.ShortDoc,
		vis:         &vis,
		pxlContents: bundleScript.Pxl,
	}

	return nil
}

func (s *Server) addScript(name string, bundleScript *pixieScript, hasLiveView bool) {
	id := uuid.NewV5(s.SeedUUID, name)
	s.store.Scripts[id] = &scriptModel{
		name:        name,
		desc:        bundleScript.ShortDoc,
		pxl:         bundleScript.Pxl,
		hasLiveView: hasLiveView,
	}
}

func (s *Server) updateStore() error {
	b, err := getBundle(s.sc, s.bundleBucket, s.bundlePath)
	if err != nil {
		return err
	}
	var errorMsgs []string
	for name, bundleScript := range b.Scripts {
		hasLiveView := bundleScript.Vis != ""
		s.addScript(name, bundleScript, hasLiveView)
		if hasLiveView {
			err = s.addLiveView(name, bundleScript)
			if err != nil {
				errorMsgs = append(errorMsgs, fmt.Sprintf("Error in Live View %s: %s", name, err.Error()))
			}
		}
	}

	if len(errorMsgs) > 0 {
		return fmt.Errorf("Encountered %d errors: %s", len(errorMsgs), strings.Join(errorMsgs, "\n"))
	}

	return nil
}

func (s *Server) storeUpdater() {
	t := time.NewTicker(time.Minute)
	ctx := context.Background()
	for range t.C {
		log.Trace("Checking if bundle needs updating...")
		attrs, err := s.sc.Bucket(s.bundleBucket).Object(s.bundlePath).Attrs(ctx)
		if err != nil {
			log.WithError(err).
				WithField("bucket", s.bundleBucket).
				WithField("path", s.bundlePath).
				Error("Failed to get attrs of bundle.json")
			continue
		}
		if attrs.Updated.After(s.storeLastUpdate) {
			log.Trace("Update to bundle required. Updating...")
			err := s.updateStore()
			if err != nil {
				log.WithError(err).
					WithField("bucket", s.bundleBucket).
					WithField("path", s.bundlePath).
					Error("Failed to update bundle.json from gcs.")
			}
			log.
				WithField("scripts", s.store.Scripts).
				WithField("live views", s.store.LiveViews).
				Trace("Finished updating bundle.")
			s.storeLastUpdate = attrs.Updated
		}
	}
}

// Start starts the store updater goroutine which checks for updates to the bundle.json.
func (s *Server) Start() {
	go s.storeUpdater()
}

// GetLiveViews returns a list of all available live views.
func (s *Server) GetLiveViews(ctx context.Context, req *scriptmgrpb.GetLiveViewsReq) (*scriptmgrpb.GetLiveViewsResp, error) {
	resp := &scriptmgrpb.GetLiveViewsResp{}
	for id, liveView := range s.store.LiveViews {
		resp.LiveViews = append(resp.LiveViews, &scriptmgrpb.LiveViewMetadata{
			Name: liveView.name,
			Desc: liveView.desc,
			ID:   utils.ProtoFromUUID(id),
		})
	}
	return resp, nil
}

// GetLiveViewContents returns the pxl script, vis info, and metdata for a live view.
func (s *Server) GetLiveViewContents(ctx context.Context, req *scriptmgrpb.GetLiveViewContentsReq) (*scriptmgrpb.GetLiveViewContentsResp, error) {
	id := utils.UUIDFromProtoOrNil(req.LiveViewID)
	if id == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "Invalid LiveViewID, bytes couldn't be parsed as UUID.")
	}
	liveView, ok := s.store.LiveViews[id]
	if !ok {
		return nil, status.Errorf(codes.InvalidArgument, "LiveViewID: %s, not found.", id.String())
	}

	return &scriptmgrpb.GetLiveViewContentsResp{
		Metadata: &scriptmgrpb.LiveViewMetadata{
			ID:   utils.ProtoFromUUID(id),
			Name: liveView.name,
			Desc: liveView.desc,
		},
		PxlContents: liveView.pxlContents,
		Vis:         liveView.vis,
	}, nil
}

// GetScripts returns a list of all available scripts.
func (s *Server) GetScripts(ctx context.Context, req *scriptmgrpb.GetScriptsReq) (*scriptmgrpb.GetScriptsResp, error) {
	resp := &scriptmgrpb.GetScriptsResp{}
	for id, script := range s.store.Scripts {
		resp.Scripts = append(resp.Scripts, &scriptmgrpb.ScriptMetadata{
			ID:          utils.ProtoFromUUID(id),
			Name:        script.name,
			Desc:        script.desc,
			HasLiveView: script.hasLiveView,
		})
	}
	return resp, nil
}

// GetScriptContents returns the pxl string of the script.
func (s *Server) GetScriptContents(ctx context.Context, req *scriptmgrpb.GetScriptContentsReq) (*scriptmgrpb.GetScriptContentsResp, error) {
	id := utils.UUIDFromProtoOrNil(req.ScriptID)
	if id == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "Invalid ScriptID, bytes couldn't be parsed as UUID.")
	}
	script, ok := s.store.Scripts[id]
	if !ok {
		return nil, status.Errorf(codes.InvalidArgument, "ScriptID: %s, not found.", id.String())
	}
	return &scriptmgrpb.GetScriptContentsResp{
		Metadata: &scriptmgrpb.ScriptMetadata{
			ID:          utils.ProtoFromUUID(id),
			Name:        script.name,
			Desc:        script.desc,
			HasLiveView: script.hasLiveView,
		},
		Contents: script.pxl,
	}, nil
}
