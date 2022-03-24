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
	"fmt"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
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

			sr := New(nc, nil)

			// Subscribe to request channel.
			mdSub, err := nc.Subscribe(CronScriptChecksumRequestChannel, func(msg *nats.Msg) {
				v2cMsg := &cvmsgspb.V2CMessage{}
				err := proto.Unmarshal(msg.Data, v2cMsg)
				require.NoError(t, err)
				req := &cvmsgspb.GetCronScriptsChecksumRequest{}
				err = types.UnmarshalAny(v2cMsg.Msg, req)
				require.NoError(t, err)
				topic := req.Topic

				checksum, err := checksumFromScriptMap(test.cloudScripts)
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

	sr := New(nc, nil)

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
