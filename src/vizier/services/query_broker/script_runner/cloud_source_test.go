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
	"reflect"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/scripts"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

func TestCloudScriptsSource_InitialState(t *testing.T) {
	tests := []struct {
		name             string
		persistedScripts map[string]*cvmsgspb.CronScript
		cloudScripts     map[string]*cvmsgspb.CronScript
		checksumMatch    bool
	}{
		{
			name: "checksums match",
			persistedScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": {
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
				"223e4567-e89b-12d3-a456-426655440000": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 6,
				},
				"223e4567-e89b-12d3-a456-426655440001": {
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
				"223e4567-e89b-12d3-a456-426655440000": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
					Script:     "px.display()",
					Configs:    "config1",
					FrequencyS: 5,
				},
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script",
					Configs:    "config2",
					FrequencyS: 22,
				},
			},
			cloudScripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440000": {
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
			persistedScripts := map[uuid.UUID]*cvmsgspb.CronScript{}
			for id, script := range test.persistedScripts {
				persistedScripts[uuid.Must(uuid.FromString(id))] = script
			}
			fcs := &fakeCronStore{scripts: persistedScripts}

			cloudScripts := test.cloudScripts
			checksumSub, _ := setupChecksumSubscription(t, nc, cloudScripts)
			defer func() {
				require.NoError(t, checksumSub.Unsubscribe())
			}()

			scriptSub, gotCronScripts := setupCloudScriptsSubscription(t, nc, cloudScripts)
			defer func() {
				require.NoError(t, scriptSub.Unsubscribe())
			}()

			source := NewCloudSource(nc, fcs, "test")
			initialScripts, err := source.Start(context.Background(), nil)
			require.NoError(t, err)
			defer source.Stop()

			require.Equal(t, test.cloudScripts, initialScripts)
			storedScripts := map[string]*cvmsgspb.CronScript{}
			for id, script := range fcs.scripts {
				storedScripts[id.String()] = script
			}
			require.Equal(t, test.cloudScripts, storedScripts)

			select {
			case <-gotCronScripts:
				if test.checksumMatch {
					t.Fatal("should not have fetched cron scripts")
				}
			case <-time.After(time.Millisecond):
				if !test.checksumMatch {
					t.Fatal("should have fetched cron scripts")
				}
			}
		})
	}

	t.Run("does not receive updates after the metadata store fails to fetch the initial state", func(t *testing.T) {
		nc, natsCleanup := testingutils.MustStartTestNATS(t)
		defer natsCleanup()
		scs := &stubCronScriptStore{GetScriptsError: errors.New("failed to get scripts")}

		checksumSub, _ := setupChecksumSubscription(t, nc, nil)
		defer func() {
			require.NoError(t, checksumSub.Unsubscribe())
		}()

		sentUpdates := []*cvmsgspb.CronScriptUpdate{
			{
				Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
					UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
						Script: &cvmsgspb.CronScript{
							ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
							Script:     "test script 1",
							Configs:    "config1",
							FrequencyS: 123,
						},
					},
				},
				RequestID: "1",
				Timestamp: 1,
			},
		}
		cronScriptResSubs, gotCronScriptResponses := setupCronScriptResponses(t, nc, sentUpdates)
		defer func() {
			for _, sub := range cronScriptResSubs {
				require.NoError(t, sub.Unsubscribe())
			}
		}()

		updatesCh := mockUpdatesCh()
		source := NewCloudSource(nc, scs, "test")
		_, err := source.Start(context.Background(), updatesCh)
		require.Error(t, err)

		sendUpdates(t, nc, sentUpdates)

		requireNoReceive(t, updatesCh, time.Millisecond)
		for _, getCronScriptResponse := range gotCronScriptResponses {
			requireNoReceive(t, getCronScriptResponse, time.Millisecond)
		}
	})

	t.Run("does not receive updates after the metadata store fails to save the initial state", func(t *testing.T) {
		persistedScripts := map[string]*cvmsgspb.CronScript{
			"223e4567-e89b-12d3-a456-426655440000": {
				ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
				Script:     "px.display()",
				Configs:    "config1",
				FrequencyS: 5,
			},
			"223e4567-e89b-12d3-a456-426655440001": {
				ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
				Script:     "test script",
				Configs:    "config2",
				FrequencyS: 22,
			},
		}
		cloudScripts := map[string]*cvmsgspb.CronScript{
			"223e4567-e89b-12d3-a456-426655440000": {
				ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
				Script:     "px.display()",
				Configs:    "config1",
				FrequencyS: 6,
			},
			"223e4567-e89b-12d3-a456-426655440001": {
				ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
				Script:     "test script",
				Configs:    "config2",
				FrequencyS: 22,
			},
		}
		nc, natsCleanup := testingutils.MustStartTestNATS(t)
		defer natsCleanup()
		scs := &stubCronScriptStore{
			GetScriptsResponse: &metadatapb.GetScriptsResponse{Scripts: persistedScripts},
			SetScriptsError:    errors.New("could not save scripts"),
		}

		checksumSub, _ := setupChecksumSubscription(t, nc, cloudScripts)
		defer func() {
			require.NoError(t, checksumSub.Unsubscribe())
		}()

		scriptSub, _ := setupCloudScriptsSubscription(t, nc, cloudScripts)
		defer func() {
			require.NoError(t, scriptSub.Unsubscribe())
		}()

		sentUpdates := []*cvmsgspb.CronScriptUpdate{
			{
				Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
					UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
						Script: &cvmsgspb.CronScript{
							ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
							Script:     "test script 1",
							Configs:    "config1",
							FrequencyS: 123,
						},
					},
				},
				RequestID: "1",
				Timestamp: 1,
			},
		}
		cronScriptResSubs, gotCronScriptResponses := setupCronScriptResponses(t, nc, sentUpdates)
		defer func() {
			for _, sub := range cronScriptResSubs {
				require.NoError(t, sub.Unsubscribe())
			}
		}()

		updatesCh := mockUpdatesCh()
		source := NewCloudSource(nc, scs, "test")
		_, err := source.Start(context.Background(), updatesCh)
		require.Error(t, err)

		sendUpdates(t, nc, sentUpdates)

		requireNoReceive(t, updatesCh, time.Millisecond)
		for _, getCronScriptResponse := range gotCronScriptResponses {
			requireNoReceive(t, getCronScriptResponse, time.Millisecond)
		}
	})
}

func TestCloudScriptsSource_Updates(t *testing.T) {
	tests := []struct {
		name    string
		scripts map[string]*cvmsgspb.CronScript
		updates []*cvmsgspb.CronScriptUpdate
	}{
		{
			name: "add one, delete one",
			scripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script 1",
					Configs:    "config1",
					FrequencyS: 123,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
								Script:     "test script 1",
								Configs:    "config1",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
			},
		},
		{
			name: "update one, delete one",
			scripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440002": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
					Script:     "test script 1",
					Configs:    "config1",
					FrequencyS: 123,
				},
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script 1",
					Configs:    "config1",
					FrequencyS: 123,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
								Script:     "test script 2",
								Configs:    "config2",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
			},
		},
		{
			name:    "no scripts",
			scripts: map[string]*cvmsgspb.CronScript{},
			updates: []*cvmsgspb.CronScriptUpdate{
				{
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
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440003"),
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			nc, natsCleanup := testingutils.MustStartTestNATS(t)
			defer natsCleanup()
			persistedScripts := map[uuid.UUID]*cvmsgspb.CronScript{}
			for id, script := range test.scripts {
				persistedScripts[uuid.Must(uuid.FromString(id))] = script
			}
			fcs := &fakeCronStore{scripts: persistedScripts}

			checksumSub, gotChecksumReq := setupChecksumSubscription(t, nc, test.scripts)
			defer func() {
				require.NoError(t, checksumSub.Unsubscribe())
			}()

			cronScriptResSubs, gotCronScriptResponses := setupCronScriptResponses(t, nc, test.updates)
			defer func() {
				for _, sub := range cronScriptResSubs {
					require.NoError(t, sub.Unsubscribe())
				}
			}()

			updatesCh := mockUpdatesCh()
			source := NewCloudSource(nc, fcs, "test")
			_, err := source.Start(context.Background(), updatesCh)
			require.NoError(t, err)
			defer source.Stop()

			<-gotChecksumReq
			sendUpdates(t, nc, test.updates)

			missing := append([]*cvmsgspb.CronScriptUpdate{}, test.updates...)
			var unexpected []*cvmsgspb.CronScriptUpdate
			var actualUpdates []*cvmsgspb.CronScriptUpdate
			for i := 0; i < len(test.updates); i++ {
				select {
				case actual := <-updatesCh:
					actualUpdates = append(actualUpdates, actual)
					anyMatches := false
					for i, expected := range missing {
						if reflect.DeepEqual(expected, actual) {
							missing[i] = missing[len(missing)-1]
							missing = missing[:len(missing)-1]
							anyMatches = true
							break
						}
					}
					if !anyMatches {
						unexpected = append(unexpected, actual)
					}
				case <-time.After(time.Millisecond):
					break
				}
			}
			require.Empty(t, missing, "missing updates")
			require.Empty(t, unexpected, "unexpected updates")

			for _, update := range actualUpdates {
				requireReceiveWithin(t, gotCronScriptResponses[update.RequestID], time.Millisecond)
				switch update.Msg.(type) {
				case *cvmsgspb.CronScriptUpdate_UpsertReq:
					req := update.GetUpsertReq()
					require.Contains(t, fcs.Scripts(), utils.UUIDFromProtoOrNil(req.GetScript().GetID()))
				case *cvmsgspb.CronScriptUpdate_DeleteReq:
					req := update.GetDeleteReq()
					require.NotContains(t, fcs.Scripts(), utils.UUIDFromProtoOrNil(req.GetScriptID()))
				}
			}
		})
	}

	t.Run("does not send any further updates after stopping", func(t *testing.T) {
		nc, natsCleanup := testingutils.MustStartTestNATS(t)
		defer natsCleanup()
		fcs := &fakeCronStore{scripts: nil}

		checksumSub, _ := setupChecksumSubscription(t, nc, nil)
		defer func() {
			require.NoError(t, checksumSub.Unsubscribe())
		}()

		sentUpdates := []*cvmsgspb.CronScriptUpdate{
			{
				Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
					UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
						Script: &cvmsgspb.CronScript{
							ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440002"),
							Script:     "test script 1",
							Configs:    "config1",
							FrequencyS: 123,
						},
					},
				},
				RequestID: "1",
				Timestamp: 1,
			},
		}
		cronScriptResSubs, gotCronScriptResponses := setupCronScriptResponses(t, nc, sentUpdates)
		defer func() {
			for _, sub := range cronScriptResSubs {
				require.NoError(t, sub.Unsubscribe())
			}
		}()

		updatesCh := mockUpdatesCh()
		source := NewCloudSource(nc, fcs, "test")
		_, err := source.Start(context.Background(), updatesCh)
		require.NoError(t, err)
		source.Stop()

		sendUpdates(t, nc, sentUpdates)

		requireNoReceive(t, updatesCh, time.Millisecond)
		for _, getCronScriptResponse := range gotCronScriptResponses {
			requireNoReceive(t, getCronScriptResponse, time.Millisecond)
		}
	})
}

func TestCloudScriptsSource_UpdateOrdering(t *testing.T) {
	tests := []struct {
		name            string
		expectedUpdates int
		scripts         map[string]*cvmsgspb.CronScript
		updates         []*cvmsgspb.CronScriptUpdate
	}{
		{
			name:            "old upsert after delete",
			expectedUpdates: 1,
			scripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script 1",
					Configs:    "config1",
					FrequencyS: 123,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
								Script:     "test script 2",
								Configs:    "config2",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
			},
		},
		{
			name:            "old upsert after upsert",
			expectedUpdates: 1,
			scripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script 1",
					Configs:    "config1",
					FrequencyS: 123,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
								Script:     "test script 3",
								Configs:    "config3",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "3",
					Timestamp: 3,
				},
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
								Script:     "test script 2",
								Configs:    "config2",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
			},
		},
		{
			name:            "old delete after upsert",
			expectedUpdates: 1,
			scripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script 1",
					Configs:    "config1",
					FrequencyS: 123,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
								Script:     "test script 2",
								Configs:    "config2",
								FrequencyS: 123,
							},
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
			},
		},
		{
			name:            "old delete after delete",
			expectedUpdates: 1,
			scripts: map[string]*cvmsgspb.CronScript{
				"223e4567-e89b-12d3-a456-426655440001": {
					ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
					Script:     "test script 1",
					Configs:    "config1",
					FrequencyS: 123,
				},
			},
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
						},
					},
					RequestID: "2",
					Timestamp: 2,
				},
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
						},
					},
					RequestID: "1",
					Timestamp: 1,
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			nc, natsCleanup := testingutils.MustStartTestNATS(t)
			defer natsCleanup()
			persistedScripts := map[uuid.UUID]*cvmsgspb.CronScript{}
			for id, script := range test.scripts {
				persistedScripts[uuid.Must(uuid.FromString(id))] = script
			}
			fcs := &fakeCronStore{scripts: persistedScripts}

			checksumSub, gotChecksumReq := setupChecksumSubscription(t, nc, test.scripts)
			defer func() {
				require.NoError(t, checksumSub.Unsubscribe())
			}()

			cronScriptResSubs, _ := setupCronScriptResponses(t, nc, test.updates)
			defer func() {
				for _, sub := range cronScriptResSubs {
					require.NoError(t, sub.Unsubscribe())
				}
			}()

			updatesCh := mockUpdatesCh()
			source := NewCloudSource(nc, fcs, "test")
			_, err := source.Start(context.Background(), updatesCh)
			require.NoError(t, err)
			defer source.Stop()

			<-gotChecksumReq
			sendUpdates(t, nc, test.updates)

			actualUpdates := countAllMessages(updatesCh)

			require.Equalf(t, test.expectedUpdates, actualUpdates, "expected %d updates, but got %d", test.expectedUpdates, actualUpdates)
		})
	}
}

func countAllMessages(updatesCh chan *cvmsgspb.CronScriptUpdate) int {
	actualUpdates := 0
	for {
		select {
		case <-updatesCh:
			actualUpdates++
		case <-time.After(time.Millisecond):
			return actualUpdates
		}
	}
}

func sendUpdates(t *testing.T, nc *nats.Conn, updates []*cvmsgspb.CronScriptUpdate) {
	for _, update := range updates {
		updateMsg, err := types.MarshalAny(update)
		require.NoError(t, err)
		c2vMsg := cvmsgspb.C2VMessage{Msg: updateMsg}
		data, err := c2vMsg.Marshal()
		require.NoError(t, err)
		require.NoError(t, nc.Publish(CronScriptUpdatesChannel, data))
	}
}

type stubCronScriptStore struct {
	GetScriptsResponse *metadatapb.GetScriptsResponse
	GetScriptsError    error

	AddOrUpdateScriptResponse *metadatapb.AddOrUpdateScriptResponse
	AddOrUpdateScriptError    error

	DeleteScriptResponse *metadatapb.DeleteScriptResponse
	DeleteScriptError    error

	SetScriptsResponse *metadatapb.SetScriptsResponse
	SetScriptsError    error

	RecordExecutionResultResponse *metadatapb.RecordExecutionResultResponse
	RecordExecutionResultError    error

	GetAllExecutionResultsResponse *metadatapb.GetAllExecutionResultsResponse
	GetAllExecutionResultsError    error
}

func (s *stubCronScriptStore) GetScripts(_ context.Context, _ *metadatapb.GetScriptsRequest, _ ...grpc.CallOption) (*metadatapb.GetScriptsResponse, error) {
	return s.GetScriptsResponse, s.GetScriptsError
}

func (s *stubCronScriptStore) AddOrUpdateScript(_ context.Context, _ *metadatapb.AddOrUpdateScriptRequest, _ ...grpc.CallOption) (*metadatapb.AddOrUpdateScriptResponse, error) {
	return s.AddOrUpdateScriptResponse, s.AddOrUpdateScriptError
}

func (s *stubCronScriptStore) DeleteScript(_ context.Context, _ *metadatapb.DeleteScriptRequest, _ ...grpc.CallOption) (*metadatapb.DeleteScriptResponse, error) {
	return s.DeleteScriptResponse, s.DeleteScriptError
}

func (s *stubCronScriptStore) SetScripts(_ context.Context, _ *metadatapb.SetScriptsRequest, _ ...grpc.CallOption) (*metadatapb.SetScriptsResponse, error) {
	return s.SetScriptsResponse, s.SetScriptsError
}

func (s *stubCronScriptStore) RecordExecutionResult(_ context.Context, _ *metadatapb.RecordExecutionResultRequest, _ ...grpc.CallOption) (*metadatapb.RecordExecutionResultResponse, error) {
	return s.RecordExecutionResultResponse, s.RecordExecutionResultError
}

func (s *stubCronScriptStore) GetAllExecutionResults(_ context.Context, _ *metadatapb.GetAllExecutionResultsRequest, _ ...grpc.CallOption) (*metadatapb.GetAllExecutionResultsResponse, error) {
	return s.GetAllExecutionResultsResponse, s.GetAllExecutionResultsError
}

func setupChecksumSubscription(t *testing.T, nc *nats.Conn, cloudScripts map[string]*cvmsgspb.CronScript) (*nats.Subscription, chan struct{}) {
	gotChecksumReq := make(chan struct{}, 1)
	checksumSub, err := nc.Subscribe(CronScriptChecksumRequestChannel, func(msg *nats.Msg) {
		v2cMsg := &cvmsgspb.V2CMessage{}
		err := proto.Unmarshal(msg.Data, v2cMsg)
		require.NoError(t, err)
		req := &cvmsgspb.GetCronScriptsChecksumRequest{}
		err = types.UnmarshalAny(v2cMsg.Msg, req)
		require.NoError(t, err)
		topic := req.Topic

		checksum, err := scripts.ChecksumFromScriptMap(cloudScripts)
		require.NoError(t, err)

		resp := &cvmsgspb.GetCronScriptsChecksumResponse{
			Checksum: checksum,
		}
		respMsg, err := types.MarshalAny(resp)
		require.NoError(t, err)
		c2vMsg := cvmsgspb.C2VMessage{
			Msg: respMsg,
		}
		data, err := c2vMsg.Marshal()
		require.NoError(t, err)

		err = nc.Publish(fmt.Sprintf("%s:%s", CronScriptChecksumResponseChannel, topic), data)
		require.NoError(t, err)
		gotChecksumReq <- struct{}{}
	})
	require.NoError(t, err)
	return checksumSub, gotChecksumReq
}

func setupCronScriptResponses(t *testing.T, nc *nats.Conn, updates []*cvmsgspb.CronScriptUpdate) (map[string]*nats.Subscription, map[string]chan struct{}) {
	subs := map[string]*nats.Subscription{}
	gotResponses := map[string]chan struct{}{}
	for _, update := range updates {
		func(update *cvmsgspb.CronScriptUpdate) {
			var err error
			gotResponses[update.RequestID] = make(chan struct{}, 1)
			subs[update.RequestID], err = nc.Subscribe(fmt.Sprintf("%s:%s", CronScriptUpdatesResponseChannel, update.RequestID), func(msg *nats.Msg) {
				v2cMsg := &cvmsgspb.V2CMessage{}
				err := proto.Unmarshal(msg.Data, v2cMsg)
				require.NoError(t, err)
				switch update.Msg.(type) {
				case *cvmsgspb.CronScriptUpdate_UpsertReq:
					res := &cvmsgspb.RegisterOrUpdateCronScriptResponse{}
					err = types.UnmarshalAny(v2cMsg.Msg, res)
					require.NoError(t, err)
				case *cvmsgspb.CronScriptUpdate_DeleteReq:
					res := &cvmsgspb.DeleteCronScriptResponse{}
					err = types.UnmarshalAny(v2cMsg.Msg, res)
					require.NoError(t, err)
				default:
					t.Fatalf("unexpected conn script response %s", reflect.TypeOf(update.Msg).Name())
				}
				gotResponses[update.RequestID] <- struct{}{}
			})
			require.NoError(t, err)
		}(update)
	}
	return subs, gotResponses
}

func setupCloudScriptsSubscription(t *testing.T, nc *nats.Conn, cloudScripts map[string]*cvmsgspb.CronScript) (*nats.Subscription, chan struct{}) {
	gotCronScripts := make(chan struct{}, 1)
	scriptSub, err := nc.Subscribe(GetCronScriptsRequestChannel, func(msg *nats.Msg) {
		v2cMsg := &cvmsgspb.V2CMessage{}
		err := proto.Unmarshal(msg.Data, v2cMsg)
		require.NoError(t, err)
		req := &cvmsgspb.GetCronScriptsRequest{}
		err = types.UnmarshalAny(v2cMsg.Msg, req)
		require.NoError(t, err)

		resp := &cvmsgspb.GetCronScriptsResponse{
			Scripts: cloudScripts,
		}
		respMsg, err := types.MarshalAny(resp)
		require.NoError(t, err)
		c2vMsg := cvmsgspb.C2VMessage{
			Msg: respMsg,
		}
		data, err := c2vMsg.Marshal()
		require.NoError(t, err)

		err = nc.Publish(fmt.Sprintf("%s:%s", GetCronScriptsResponseChannel, req.Topic), data)
		require.NoError(t, err)
		gotCronScripts <- struct{}{}
	})
	require.NoError(t, err)
	return scriptSub, gotCronScripts
}
