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
	"fmt"
	"sync"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/scripts"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

func TestScriptRunner_CompareScriptState(t *testing.T) {
	tests := []struct {
		name             string
		persistedScripts map[string]*cvmsgspb.CronScript
		cloudScripts     map[string]*cvmsgspb.CronScript
		checksumMatch    bool
	}{
		{
			name: "checksums match",
			persistedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			checksumMatch: true,
		},
		{
			name: "checksums mismatch: one field different",
			persistedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 6,
				},
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			checksumMatch: false,
		},
		{
			name: "checksums mismatch: missing script",
			persistedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 6,
				},
			},
			checksumMatch: false,
		},
		{
			name:             "checksums match: empty",
			persistedScripts: map[string]*cvmsgspb.CronScript{},
			cloudScripts:     map[string]*cvmsgspb.CronScript{},
			checksumMatch:    true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			nc, natsCleanup := testingutils.MustStartTestNATS(t)
			defer natsCleanup()

			sr, err := New(nc, &fakeCronStore{scripts: map[uuid.UUID]*cvmsgspb.CronScript{}}, nil, "test")
			require.NoError(t, err)

			// Subscribe to request channel.
			mdSub, err := nc.Subscribe(CronScriptChecksumRequestChannel, func(msg *nats.Msg) {
				v2cMsg := &cvmsgspb.V2CMessage{}
				err := proto.Unmarshal(msg.Data, v2cMsg)
				require.NoError(t, err)
				req := &cvmsgspb.GetCronScriptsChecksumRequest{}
				err = types.UnmarshalAny(v2cMsg.Msg, req)
				require.NoError(t, err)
				topic := req.Topic

				checksum, err := scripts.ChecksumFromScriptMap(test.cloudScripts)
				require.NoError(t, err)
				resp := &cvmsgspb.GetCronScriptsChecksumResponse{
					Checksum: checksum,
				}
				anyMsg, err := types.MarshalAny(resp)
				require.NoError(t, err)
				c2vMsg := cvmsgspb.C2VMessage{
					Msg: anyMsg,
				}
				b, err := c2vMsg.Marshal()
				require.NoError(t, err)

				err = nc.Publish(fmt.Sprintf("%s:%s", CronScriptChecksumResponseChannel, topic), b)
				require.NoError(t, err)
			})
			defer func() {
				err = mdSub.Unsubscribe()
				require.NoError(t, err)
			}()

			match, err := sr.compareScriptState(test.persistedScripts)
			require.Nil(t, err)
			assert.Equal(t, test.checksumMatch, match)
		})
	}
}

func TestScriptRunner_GetCloudScripts(t *testing.T) {
	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	sr, err := New(nc, nil, nil, "test")
	require.NoError(t, err)

	scripts := map[string]*cvmsgspb.CronScript{
		"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
			ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
			Script:     "px.display()",
			Configs:    "config1",
			FrequencyS: 5,
		},
		"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
			ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
			Script:     "test script",
			Configs:    "config2",
			FrequencyS: 22,
		},
	}

	// Subscribe to request channel.
	mdSub, err := nc.Subscribe(GetCronScriptsRequestChannel, func(msg *nats.Msg) {
		v2cMsg := &cvmsgspb.V2CMessage{}
		err := proto.Unmarshal(msg.Data, v2cMsg)
		require.NoError(t, err)
		req := &cvmsgspb.GetCronScriptsRequest{}
		err = types.UnmarshalAny(v2cMsg.Msg, req)
		require.NoError(t, err)
		topic := req.Topic

		resp := &cvmsgspb.GetCronScriptsResponse{
			Scripts: scripts,
		}
		anyMsg, err := types.MarshalAny(resp)
		require.NoError(t, err)
		c2vMsg := cvmsgspb.C2VMessage{
			Msg: anyMsg,
		}
		b, err := c2vMsg.Marshal()
		require.NoError(t, err)

		err = nc.Publish(fmt.Sprintf("%s:%s", GetCronScriptsResponseChannel, topic), b)
		require.NoError(t, err)
	})
	defer func() {
		err = mdSub.Unsubscribe()
		require.NoError(t, err)
	}()

	resp, err := sr.getCloudScripts()
	require.Nil(t, err)
	assert.Equal(t, scripts, resp)
}

type fakeCronStore struct {
	scripts map[uuid.UUID]*cvmsgspb.CronScript
}

// GetScripts fetches all scripts in the cron script store.
func (s *fakeCronStore) GetScripts(ctx context.Context, req *metadatapb.GetScriptsRequest, opts ...grpc.CallOption) (*metadatapb.GetScriptsResponse, error) {
	m := make(map[string]*cvmsgspb.CronScript)
	for k, v := range s.scripts {
		m[k.String()] = v
	}

	return &metadatapb.GetScriptsResponse{
		Scripts: m,
	}, nil
}

// AddOrUpdateScript updates or adds a cron script to the store, based on ID.
func (s *fakeCronStore) AddOrUpdateScript(ctx context.Context, req *metadatapb.AddOrUpdateScriptRequest, opts ...grpc.CallOption) (*metadatapb.AddOrUpdateScriptResponse, error) {
	s.scripts[utils.UUIDFromProtoOrNil(req.Script.ID)] = req.Script

	return &metadatapb.AddOrUpdateScriptResponse{}, nil
}

// DeleteScript deletes a cron script from the store by ID.
func (s *fakeCronStore) DeleteScript(ctx context.Context, req *metadatapb.DeleteScriptRequest, opts ...grpc.CallOption) (*metadatapb.DeleteScriptResponse, error) {
	_, ok := s.scripts[utils.UUIDFromProtoOrNil(req.ScriptID)]
	if ok {
		delete(s.scripts, utils.UUIDFromProtoOrNil(req.ScriptID))
	}

	return &metadatapb.DeleteScriptResponse{}, nil
}

// SetScripts sets the list of all cron scripts to match the given set of scripts.
func (s *fakeCronStore) SetScripts(ctx context.Context, req *metadatapb.SetScriptsRequest, opts ...grpc.CallOption) (*metadatapb.SetScriptsResponse, error) {
	m := make(map[uuid.UUID]*cvmsgspb.CronScript)
	for k, v := range req.Scripts {
		m[uuid.FromStringOrNil(k)] = v
	}
	s.scripts = m

	return &metadatapb.SetScriptsResponse{}, nil
}

func TestScriptRunner_SyncScripts(t *testing.T) {
	tests := []struct {
		name             string
		persistedScripts map[string]*cvmsgspb.CronScript
		cloudScripts     map[string]*cvmsgspb.CronScript
		updates          []*cvmsgspb.CronScriptUpdate
		expectedScripts  map[string]*cvmsgspb.CronScript
	}{
		{
			name: "initial match",
			persistedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
			},
			expectedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440002": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
					Script:     "test script 2",
					Configs:    "config3",
					FrequencyS: 123,
				},
			},
		},
		{
			name: "initial mismatch",
			persistedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
			},
			expectedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440002": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
					Script:     "test script 2",
					Configs:    "config3",
					FrequencyS: 123,
				},
			},
		},
		{
			name:             "persisted empty",
			persistedScripts: map[string]*cvmsgspb.CronScript{},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
			},
			expectedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
				"223e4567-e89b-12d3-a456-426655440002": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
					Script:     "test script 2",
					Configs:    "config3",
					FrequencyS: 123,
				},
			},
		},
		{
			name:             "cloud empty",
			persistedScripts: map[string]*cvmsgspb.CronScript{},
			cloudScripts:     map[string]*cvmsgspb.CronScript{},
			updates: []*cvmsgspb.CronScriptUpdate{
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440004"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "3",
					Timestamp: 3,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
						},
					},
					RequestID: "4",
					Timestamp: 4,
				},
			},
			expectedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440002": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
					Script:     "test script 2",
					Configs:    "config3",
					FrequencyS: 123,
				},
				"223e4567-e89b-12d3-a456-426655440004": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440004"),
					Script:     "test script 2",
					Configs:    "config3",
					FrequencyS: 123,
				},
			},
		},
		{
			name:             "out-of-order update",
			persistedScripts: map[string]*cvmsgspb.CronScript{},
			cloudScripts:     map[string]*cvmsgspb.CronScript{},
			updates: []*cvmsgspb.CronScriptUpdate{
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440004"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "3",
					Timestamp: 3,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
						},
					},
					RequestID: "4",
					Timestamp: 4,
				},
				&cvmsgspb.CronScriptUpdate{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
								Script:     "test script 2",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "5",
					Timestamp: 2,
				},
			},
			expectedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440002": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
					Script:     "test script 2",
					Configs:    "config3",
					FrequencyS: 123,
				},
				"223e4567-e89b-12d3-a456-426655440004": &cvmsgspb.CronScript{
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440004"),
					Script:     "test script 2",
					Configs:    "config3",
					FrequencyS: 123,
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			nc, natsCleanup := testingutils.MustStartTestNATS(t)
			defer natsCleanup()

			initialScripts := make(map[uuid.UUID]*cvmsgspb.CronScript)
			for k, v := range test.persistedScripts {
				initialScripts[uuid.FromStringOrNil(k)] = v
			}

			fcs := &fakeCronStore{scripts: initialScripts}
			sr, err := New(nc, fcs, nil, "test")
			require.NoError(t, err)

			var wg sync.WaitGroup
			wg.Add(len(test.updates))

			// Subscribe to request channel.
			mdSub, err := nc.Subscribe(CronScriptChecksumRequestChannel, func(msg *nats.Msg) {
				v2cMsg := &cvmsgspb.V2CMessage{}
				err := proto.Unmarshal(msg.Data, v2cMsg)
				require.NoError(t, err)
				req := &cvmsgspb.GetCronScriptsChecksumRequest{}
				err = types.UnmarshalAny(v2cMsg.Msg, req)
				require.NoError(t, err)
				topic := req.Topic
				checksum, err := scripts.ChecksumFromScriptMap(test.cloudScripts)
				require.NoError(t, err)
				resp := &cvmsgspb.GetCronScriptsChecksumResponse{
					Checksum: checksum,
				}
				anyMsg, err := types.MarshalAny(resp)
				require.NoError(t, err)
				c2vMsg := cvmsgspb.C2VMessage{
					Msg: anyMsg,
				}
				b, err := c2vMsg.Marshal()
				require.NoError(t, err)

				err = nc.Publish(fmt.Sprintf("%s:%s", CronScriptChecksumResponseChannel, topic), b)
				require.NoError(t, err)
			})
			defer func() {
				err = mdSub.Unsubscribe()
				require.NoError(t, err)
			}()

			md2Sub, err := nc.Subscribe(GetCronScriptsRequestChannel, func(msg *nats.Msg) {
				v2cMsg := &cvmsgspb.V2CMessage{}
				err := proto.Unmarshal(msg.Data, v2cMsg)
				require.NoError(t, err)
				req := &cvmsgspb.GetCronScriptsRequest{}
				err = types.UnmarshalAny(v2cMsg.Msg, req)
				require.NoError(t, err)
				topic := req.Topic

				resp := &cvmsgspb.GetCronScriptsResponse{
					Scripts: test.cloudScripts,
				}
				anyMsg, err := types.MarshalAny(resp)
				require.NoError(t, err)
				c2vMsg := cvmsgspb.C2VMessage{
					Msg: anyMsg,
				}
				b, err := c2vMsg.Marshal()
				require.NoError(t, err)

				err = nc.Publish(fmt.Sprintf("%s:%s", GetCronScriptsResponseChannel, topic), b)
				require.NoError(t, err)
			})
			defer func() {
				err = md2Sub.Unsubscribe()
				require.NoError(t, err)
			}()

			for _, u := range test.updates {
				md3Sub, err := nc.Subscribe(fmt.Sprintf("%s:%s", CronScriptUpdatesResponseChannel, u.RequestID), func(msg *nats.Msg) {
					wg.Done()
				})
				defer func() {
					err = md3Sub.Unsubscribe()
					require.NoError(t, err)
				}()

				anyMsg, err := types.MarshalAny(u)
				require.NoError(t, err)
				c2vMsg := cvmsgspb.C2VMessage{
					Msg: anyMsg,
				}
				b, err := c2vMsg.Marshal()
				require.NoError(t, err)
				err = nc.Publish(CronScriptUpdatesChannel, b)
			}

			err = sr.SyncScripts()
			wg.Wait()
			require.NoError(t, err)
			assert.Equal(t, len(test.expectedScripts), len(fcs.scripts))
			for k, v := range test.expectedScripts {
				val, ok := fcs.scripts[uuid.FromStringOrNil(k)]
				assert.True(t, ok)
				assert.Equal(t, v, val)
			}

			assert.Equal(t, len(test.expectedScripts), len(sr.runnerMap))
			for k, v := range test.expectedScripts {
				val, ok := sr.runnerMap[uuid.FromStringOrNil(k)]
				assert.True(t, ok)
				assert.Equal(t, v, val.cronScript)
			}
		})
	}
}
