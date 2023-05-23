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
	"errors"
	"fmt"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/scripts"
	svcutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

// CloudSource constructs a [Source] that will pull cron scripts from the control plane
func CloudSource(nc *nats.Conn, csClient metadatapb.CronScriptStoreServiceClient, signingKey string) Source {
	return func(baseCtx context.Context, updateCb func(*cvmsgspb.CronScriptUpdate)) (map[string]*cvmsgspb.CronScript, func(), error) {
		ctx := metadata.AppendToOutgoingContext(baseCtx,
			"authorization", fmt.Sprintf("bearer %s", cronScriptStoreToken(signingKey)),
		)
		sub, err := nc.Subscribe(CronScriptUpdatesChannel, natsUpdater(ctx, nc, csClient, updateCb))
		if err != nil {
			return nil, nil, err
		}
		unsubscribe := func() {
			if err := sub.Unsubscribe(); err != nil {
				log.WithError(err).Error("could not unsubscribe from cloud cron script updates")
			}
		}
		initialScripts, err := fetchInitialScripts(ctx, nc, csClient)
		if err != nil {
			unsubscribe()
			return nil, nil, err
		}
		return initialScripts, unsubscribe, nil
	}
}

func cronScriptStoreToken(signingKey string) string {
	claims := svcutils.GenerateJWTForService("cron_script_store", "vizier")
	token, _ := svcutils.SignJWTClaims(claims, signingKey)
	return token
}

func natsUpdater(ctx context.Context, nc *nats.Conn, csClient metadatapb.CronScriptStoreServiceClient, updateCb func(*cvmsgspb.CronScriptUpdate)) func(msg *nats.Msg) {
	return func(msg *nats.Msg) {
		var cronScriptUpdate cvmsgspb.CronScriptUpdate
		if err := unmarshalC2V(msg, &cronScriptUpdate); err != nil {
			log.WithError(err).Error("Failed to unmarshal c2v message")
			return
		}

		updateCb(&cronScriptUpdate)

		switch cronScriptUpdate.Msg.(type) {
		case *cvmsgspb.CronScriptUpdate_UpsertReq:
			upsertReq := cronScriptUpdate.GetUpsertReq()
			_, err := csClient.AddOrUpdateScript(ctx, &metadatapb.AddOrUpdateScriptRequest{Script: upsertReq.Script})
			if err != nil {
				log.WithError(err).Error("Failed to upsert script in metadata")
			}

			// Send response.
			data, err := marshalV2C(&cvmsgspb.RegisterOrUpdateCronScriptResponse{})
			if err != nil {
				log.WithError(err).Error("Failed to marshal update script response")
				return
			}
			err = nc.Publish(fmt.Sprintf("%s:%s", CronScriptUpdatesResponseChannel, cronScriptUpdate.RequestID), data)
			if err != nil {
				log.WithError(err).Error("Failed to publish update script response")
			}
		case *cvmsgspb.CronScriptUpdate_DeleteReq:
			deleteReq := cronScriptUpdate.GetDeleteReq()
			_, err := csClient.DeleteScript(ctx, &metadatapb.DeleteScriptRequest{ScriptID: deleteReq.ScriptID})
			if err != nil {
				log.WithError(err).Error("Failed to delete script from metadata")
			}

			// Send response.
			data, err := marshalV2C(&cvmsgspb.DeleteCronScriptResponse{})
			if err != nil {
				log.WithError(err).Error("Failed to marshal update script response")
				return
			}
			if err != nil {
				log.WithError(err).Error("Failed to marshal update script response")
				return
			}
			err = nc.Publish(fmt.Sprintf("%s:%s", CronScriptUpdatesResponseChannel, cronScriptUpdate.RequestID), data)
			if err != nil {
				log.WithError(err).Error("Failed to publish update script response")
			}
		default:
			log.Error("Received unknown message for cronScriptUpdate")
		}
	}
}

func unmarshalC2V[T proto.Message](msg *nats.Msg, msgBody T) error {
	c2vMsg := &cvmsgspb.C2VMessage{}
	err := proto.Unmarshal(msg.Data, c2vMsg)
	if err != nil {
		return err
	}
	err = types.UnmarshalAny(c2vMsg.Msg, msgBody)
	if err != nil {
		return err
	}
	return err
}

func marshalV2C[T proto.Message](msg T) ([]byte, error) {
	msgBody, err := types.MarshalAny(msg)
	if err != nil {
		return nil, err
	}
	v2cMsg := &cvmsgspb.V2CMessage{Msg: msgBody}
	return v2cMsg.Marshal()
}

func fetchInitialScripts(ctx context.Context, nc *nats.Conn, csClient metadatapb.CronScriptStoreServiceClient) (map[string]*cvmsgspb.CronScript, error) {
	resp, err := csClient.GetScripts(ctx, &metadatapb.GetScriptsRequest{})
	if err != nil {
		log.WithError(err).Error("Failed to fetch initialScripts from store")
		return nil, err
	}
	initialScripts := resp.Scripts

	// Check if persisted initialScripts are up-to-date.
	upToDate, err := compareScriptState(ctx, nc, initialScripts)
	if err != nil {
		// In the case there is a failure, we should just refetch the initialScripts.
		log.WithError(err).Error("Failed to verify script checksum")
	}

	// If hash is not equal, fetch initialScripts from cloud.
	if !upToDate {
		cloudScripts, err := getCloudScripts(ctx, nc)
		if err != nil {
			log.WithError(err).Error("Failed to fetch initialScripts from cloud")
		} else {
			// Clear out persisted initialScripts.
			_, err = csClient.SetScripts(ctx, &metadatapb.SetScriptsRequest{Scripts: make(map[string]*cvmsgspb.CronScript)})
			if err != nil {
				log.WithError(err).Error("Failed to delete initialScripts from store")
				return nil, err
			}
			initialScripts = cloudScripts
		}
	}
	return initialScripts, nil
}

func compareScriptState(ctx context.Context, nc *nats.Conn, existingScripts map[string]*cvmsgspb.CronScript) (bool, error) {
	// Get hash of map.
	existingChecksum, err := scripts.ChecksumFromScriptMap(existingScripts)
	if err != nil {
		return false, err
	}

	topicID := uuid.Must(uuid.NewV4())
	req := &cvmsgspb.GetCronScriptsChecksumRequest{
		Topic: topicID.String(),
	}
	res := &cvmsgspb.GetCronScriptsChecksumResponse{}
	err = natsReplyAndResponse(ctx, nc, req, CronScriptChecksumRequestChannel, res, fmt.Sprintf("%s:%s", CronScriptChecksumResponseChannel, topicID.String()))
	if err != nil {
		return false, err
	}
	return existingChecksum == res.Checksum, nil
}

func natsReplyAndResponse(ctx context.Context, nc *nats.Conn, req proto.Message, requestTopic string, res proto.Message, responseTopic string) error {
	// Subscribe to topic that the response will be sent on.
	subCh := make(chan *nats.Msg, 4096)
	sub, err := nc.ChanSubscribe(responseTopic, subCh)
	if err != nil {
		return err
	}
	defer sub.Unsubscribe()

	// Publish request.
	reqData, err := marshalV2C(req)
	if err != nil {
		return err
	}
	err = nc.Publish(requestTopic, reqData)
	if err != nil {
		return err
	}

	// Wait for response.
	t := time.NewTimer(natsWaitTimeout)
	defer t.Stop()
	for {
		select {
		case <-ctx.Done():
			return errors.New("Cancelled")
		case msg := <-subCh:
			if err := unmarshalC2V(msg, res); err != nil {
				log.WithError(err).Error("Failed to unmarshal checksum response")
				return err
			}
			return nil
		case <-t.C:
			return errors.New("Failed to get response")
		}
	}
}

func getCloudScripts(ctx context.Context, nc *nats.Conn) (map[string]*cvmsgspb.CronScript, error) {
	topicID := uuid.Must(uuid.NewV4())
	req := &cvmsgspb.GetCronScriptsRequest{Topic: topicID.String()}
	res := &cvmsgspb.GetCronScriptsResponse{}
	err := natsReplyAndResponse(ctx, nc, req, GetCronScriptsRequestChannel, res, fmt.Sprintf("%s:%s", GetCronScriptsResponseChannel, topicID.String()))
	if err != nil {
		return nil, err
	}
	return res.Scripts, nil
}
