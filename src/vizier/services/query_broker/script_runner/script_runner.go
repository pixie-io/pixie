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

package scriptrunner

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/utils/messagebus"
)

var (
	// CronScriptChecksumRequestChannel is the NATS channel to make checksum requests to.
	CronScriptChecksumRequestChannel = messagebus.V2CTopic("GetCronScriptsCheckSumRequest")
	// CronScriptChecksumResponseChannel is the NATS channel that checksum responses are published to.
	CronScriptChecksumResponseChannel = messagebus.C2VTopic("GetCronScriptsCheckSumResponse")
	// GetCronScriptsRequestChannel is the NATS channel script requests are sent to.
	GetCronScriptsRequestChannel = messagebus.V2CTopic("GetCronScriptsRequest")
	// GetCronScriptsResponseChannel is the NATS channel that script responses are published to.
	GetCronScriptsResponseChannel = messagebus.C2VTopic("GetCronScriptsResponse")
	// CronScriptUpdatesChannel is the NATS channel that any cron script updates are published to.
	CronScriptUpdatesChannel = messagebus.C2VTopic("CronScriptsUpdates")
	// CronScriptUpdatesResponseChannel is the NATS channel that script updates are published to.
	CronScriptUpdatesResponseChannel = messagebus.V2CTopic("CronScriptsUpdatesResponse")
	natsWaitTimeout                  = 2 * time.Minute
)

// ScriptRunner tracks registered cron scripts and runs them according to schedule.
type ScriptRunner struct {
	nc          *nats.Conn
	csClient    metadatapb.CronScriptStoreServiceClient
	runnerMap   map[uuid.UUID]*runner
	runnerMapMu sync.Mutex

	done chan struct{}
	once sync.Once

	updatesCh  chan *nats.Msg
	updatesSub *nats.Subscription
}

// New creates a new script runner.
func New(nc *nats.Conn, csClient metadatapb.CronScriptStoreServiceClient) (*ScriptRunner, error) {
	updatesCh := make(chan *nats.Msg, 4096)
	sub, err := nc.ChanSubscribe(CronScriptUpdatesChannel, updatesCh)
	if err != nil {
		log.WithError(err).Error("Failed to listen for cron script updates")
		return nil, err
	}

	sr := &ScriptRunner{nc: nc, csClient: csClient, done: make(chan struct{}), updatesCh: updatesCh, updatesSub: sub, runnerMap: make(map[uuid.UUID]*runner)}
	return sr, nil
}

// Stop performs any necessary cleanup before shutdown.
func (s *ScriptRunner) Stop() {
	s.once.Do(func() {
		close(s.updatesCh)
		s.updatesSub.Unsubscribe()
		close(s.done)
	})
}

// SyncScripts syncs the known set of scripts in Vizier with scripts in Cloud.
func (s *ScriptRunner) SyncScripts() error {
	// Fetch persisted scripts.
	resp, err := s.csClient.GetScripts(context.Background(), &metadatapb.GetScriptsRequest{})
	if err != nil {
		log.WithError(err).Error("Failed to fetch scripts from store")
		return err
	}
	scripts := resp.Scripts

	// Check if persisted scripts are up-to-date.
	upToDate, err := s.compareScriptState(scripts)
	if err != nil {
		// In the case there is a failure, we should just refetch the scripts.
		log.WithError(err).Error("Failed to verify script checksum")
	}

	// If hash is not equal, fetch scripts from cloud.
	if !upToDate {
		cloudScripts, err := s.getCloudScripts()
		if err != nil {
			log.WithError(err).Error("Failed to fetch scripts from cloud")
			return err
		}
		// Clear out persisted scripts.
		_, err = s.csClient.SetScripts(context.Background(), &metadatapb.SetScriptsRequest{Scripts: make(map[string]*cvmsgspb.CronScript)})
		if err != nil {
			log.WithError(err).Error("Failed to delete scripts from store")
			return err
		}
		scripts = cloudScripts
	}

	// Add runners.
	for k, v := range scripts {
		err = s.upsertScript(uuid.FromStringOrNil(k), v)
		if err != nil {
			log.WithError(err).Error("Failed to upsert script, skipping...")
		}
	}
	go s.processUpdates()
	return nil
}

func (s *ScriptRunner) processUpdates() {
	for {
		select {
		case <-s.done:
			return
		case msg := <-s.updatesCh:
			c2vMsg := &cvmsgspb.C2VMessage{}
			err := proto.Unmarshal(msg.Data, c2vMsg)
			if err != nil {
				log.WithError(err).Error("Failed to unmarshal c2v message")
				continue
			}
			resp := &cvmsgspb.CronScriptUpdate{}
			err = types.UnmarshalAny(c2vMsg.Msg, resp)
			if err != nil {
				log.WithError(err).Error("Failed to unmarshal c2v message")
				continue
			}
			switch resp.Msg.(type) {
			case *cvmsgspb.CronScriptUpdate_UpsertReq:
				uResp := resp.GetUpsertReq()
				err := s.upsertScript(utils.UUIDFromProtoOrNil(uResp.Script.ID), uResp.Script)
				if err != nil {
					log.WithError(err).Error("Failed to upsert script")
				}
				// Send response.
				r := &cvmsgspb.RegisterOrUpdateCronScriptResponse{}
				reqAnyMsg, err := types.MarshalAny(r)
				if err != nil {
					log.WithError(err).Error("Failed to marshal update script response")
					continue
				}
				v2cMsg := cvmsgspb.V2CMessage{
					Msg: reqAnyMsg,
				}
				// Publish request.
				b, err := v2cMsg.Marshal()
				if err != nil {
					log.WithError(err).Error("Failed to marshal update script response")
					continue
				}
				err = s.nc.Publish(fmt.Sprintf("%s:%s", CronScriptUpdatesResponseChannel, resp.RequestID), b)
				if err != nil {
					log.WithError(err).Error("Failed to publish update script response")
				}
			case *cvmsgspb.CronScriptUpdate_DeleteReq:
				dResp := resp.GetDeleteReq()
				err := s.deleteScript(utils.UUIDFromProtoOrNil(dResp.ScriptID))
				if err != nil {
					log.WithError(err).Error("Failed to delete script")
				}
				// Send response.
				r := &cvmsgspb.DeleteCronScriptResponse{}
				reqAnyMsg, err := types.MarshalAny(r)
				if err != nil {
					log.WithError(err).Error("Failed to marshal update script response")
					continue
				}
				v2cMsg := cvmsgspb.V2CMessage{
					Msg: reqAnyMsg,
				}
				// Publish request.
				b, err := v2cMsg.Marshal()
				if err != nil {
					log.WithError(err).Error("Failed to marshal update script response")
					continue
				}
				err = s.nc.Publish(fmt.Sprintf("%s:%s", CronScriptUpdatesResponseChannel, resp.RequestID), b)
				if err != nil {
					log.WithError(err).Error("Failed to publish update script response")
				}
			default:
				log.Error("Received unknown message for cronScriptUpdate")
			}
		}
	}
}

func (s *ScriptRunner) upsertScript(id uuid.UUID, script *cvmsgspb.CronScript) error {
	s.runnerMapMu.Lock()
	defer s.runnerMapMu.Unlock()

	if v, ok := s.runnerMap[id]; ok {
		v.stop()
		delete(s.runnerMap, id)
	}
	r := newRunner(script)
	s.runnerMap[id] = r
	go r.start()
	_, err := s.csClient.AddOrUpdateScript(context.Background(), &metadatapb.AddOrUpdateScriptRequest{Script: script})
	if err != nil {
		log.WithError(err).Error("Failed to upsert script in metadata")
	}

	return nil
}

func (s *ScriptRunner) deleteScript(id uuid.UUID) error {
	s.runnerMapMu.Lock()
	defer s.runnerMapMu.Unlock()
	v, ok := s.runnerMap[id]
	if !ok {
		return nil
	}
	v.stop()
	delete(s.runnerMap, id)
	_, err := s.csClient.DeleteScript(context.Background(), &metadatapb.DeleteScriptRequest{ScriptID: utils.ProtoFromUUID(id)})
	if err != nil {
		log.WithError(err).Error("Failed to delete script from metadata")
	}

	return nil
}

func (s *ScriptRunner) compareScriptState(existingScripts map[string]*cvmsgspb.CronScript) (bool, error) {
	// Get hash of map.
	existingChecksum, err := checksumFromScriptMap(existingScripts)
	if err != nil {
		return false, err
	}

	topicID := uuid.Must(uuid.NewV4())
	req := &cvmsgspb.GetCronScriptsChecksumRequest{
		Topic: topicID.String(),
	}
	reqAnyMsg, err := types.MarshalAny(req)
	if err != nil {
		return false, err
	}
	v2cMsg := cvmsgspb.V2CMessage{
		Msg: reqAnyMsg,
	}
	c2vMsg, err := s.natsReplyAndResponse(&v2cMsg, CronScriptChecksumRequestChannel, fmt.Sprintf("%s:%s", CronScriptChecksumResponseChannel, topicID.String()))
	if err != nil {
		return false, err
	}

	resp := &cvmsgspb.GetCronScriptsChecksumResponse{}
	err = types.UnmarshalAny(c2vMsg.Msg, resp)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal checksum response")
		return false, err
	}
	return existingChecksum == resp.Checksum, nil
}

func (s *ScriptRunner) getCloudScripts() (map[string]*cvmsgspb.CronScript, error) {
	topicID := uuid.Must(uuid.NewV4())
	req := &cvmsgspb.GetCronScriptsRequest{
		Topic: topicID.String(),
	}
	reqAnyMsg, err := types.MarshalAny(req)
	if err != nil {
		return nil, err
	}
	v2cMsg := cvmsgspb.V2CMessage{
		Msg: reqAnyMsg,
	}

	c2vMsg, err := s.natsReplyAndResponse(&v2cMsg, GetCronScriptsRequestChannel, fmt.Sprintf("%s:%s", GetCronScriptsResponseChannel, topicID.String()))
	if err != nil {
		return nil, err
	}

	resp := &cvmsgspb.GetCronScriptsResponse{}
	err = types.UnmarshalAny(c2vMsg.Msg, resp)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal checksum response")
		return nil, err
	}
	return resp.Scripts, nil
}

func (s *ScriptRunner) natsReplyAndResponse(req *cvmsgspb.V2CMessage, requestTopic string, responseTopic string) (*cvmsgspb.C2VMessage, error) {
	// Subscribe to topic that the response will be sent on.
	subCh := make(chan *nats.Msg, 4096)
	sub, err := s.nc.ChanSubscribe(responseTopic, subCh)
	if err != nil {
		return nil, err
	}
	defer sub.Unsubscribe()

	// Publish request.
	b, err := req.Marshal()
	if err != nil {
		return nil, err
	}

	err = s.nc.Publish(requestTopic, b)
	if err != nil {
		return nil, err
	}

	// Wait for response.
	t := time.NewTimer(natsWaitTimeout)
	for {
		select {
		case <-s.done:
			return nil, errors.New("Cancelled")
		case msg := <-subCh:
			c2vMsg := &cvmsgspb.C2VMessage{}
			err := proto.Unmarshal(msg.Data, c2vMsg)
			if err != nil {
				log.WithError(err).Error("Failed to unmarshal c2v message")
				return nil, err
			}
			return c2vMsg, nil
		case <-t.C:
			return nil, errors.New("Failed to get response")
		}
	}
}

func checksumFromScriptMap(scripts map[string]*cvmsgspb.CronScript) (string, error) {
	scriptStr, err := json.Marshal(scripts)
	if err != nil {
		log.WithError(err).Error("Failed to get checksum")
		return "", err
	}
	h := sha256.New()
	h.Write([]byte(scriptStr))

	return string(h.Sum(nil)), nil
}

// Logic for "runners" which handle the script execution.
type runner struct {
	cronScript *cvmsgspb.CronScript

	done chan struct{}
	once sync.Once
}

func newRunner(script *cvmsgspb.CronScript) *runner {
	return &runner{
		cronScript: script, done: make(chan struct{}),
	}
}

func (r *runner) start() {
}

func (r *runner) stop() {
	r.once.Do(func() {
		close(r.done)
	})
}
