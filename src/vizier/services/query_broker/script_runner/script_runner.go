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
	natsWaitTimeout               = 2 * time.Minute
)

// ScriptRunner tracks registered cron scripts and runs them according to schedule.
type ScriptRunner struct {
	nc       *nats.Conn
	csClient metadatapb.CronScriptStoreServiceClient

	done chan struct{}
	once sync.Once
}

// New creates a new script runner.
func New(nc *nats.Conn, csClient metadatapb.CronScriptStoreServiceClient) *ScriptRunner {
	return &ScriptRunner{nc: nc, csClient: csClient, done: make(chan struct{})}
}

// Stop performs any necessary cleanup before shutdown.
func (s *ScriptRunner) Stop() {
	s.once.Do(func() {
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
		scripts = cloudScripts
		_, err = s.csClient.SetScripts(context.Background(), &metadatapb.SetScriptsRequest{Scripts: cloudScripts})
		if err != nil {
			log.WithError(err).Error("Failed to persist current cron scripts")
		}
	}
	_ = scripts

	// Listen for changes to any new scripts.

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
