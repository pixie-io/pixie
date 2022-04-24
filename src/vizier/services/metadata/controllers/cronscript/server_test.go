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

package cronscript_test

import (
	"context"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/controllers/cronscript"
	mock_cronscript "px.dev/pixie/src/vizier/services/metadata/controllers/cronscript/mock"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

func TestGetScripts(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockStore := mock_cronscript.NewMockStore(ctrl)

	s1 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	}

	s2 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
	}

	mockStore.EXPECT().GetCronScripts().Return([]*cvmsgspb.CronScript{s1, s2}, nil)

	s := cronscript.New(mockStore)

	resp, err := s.GetScripts(context.Background(), &metadatapb.GetScriptsRequest{})
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, map[string]*cvmsgspb.CronScript{
		"223e4567-e89b-12d3-a456-426655440000": s1,
		"223e4567-e89b-12d3-a456-426655440001": s2,
	}, resp.Scripts)
}

func TestAddOrUpdateScript(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockStore := mock_cronscript.NewMockStore(ctrl)

	s1 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	}

	mockStore.EXPECT().UpsertCronScript(s1).Return(nil)

	s := cronscript.New(mockStore)

	resp, err := s.AddOrUpdateScript(context.Background(), &metadatapb.AddOrUpdateScriptRequest{
		Script: s1,
	})
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &metadatapb.AddOrUpdateScriptResponse{}, resp)
}

func TestDeleteScript(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockStore := mock_cronscript.NewMockStore(ctrl)

	mockStore.EXPECT().DeleteCronScript(uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000")).Return(nil)

	s := cronscript.New(mockStore)

	resp, err := s.DeleteScript(context.Background(), &metadatapb.DeleteScriptRequest{
		ScriptID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	})
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &metadatapb.DeleteScriptResponse{}, resp)
}

func TestSetScripts(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockStore := mock_cronscript.NewMockStore(ctrl)

	s1 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	}

	s2 := &cvmsgspb.CronScript{
		ID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440001"),
	}

	mockStore.EXPECT().SetCronScripts(gomock.Any()).
		DoAndReturn(func(scripts []*cvmsgspb.CronScript) error {
			assert.ElementsMatch(t, []*cvmsgspb.CronScript{s1, s2}, scripts)
			return nil
		})

	s := cronscript.New(mockStore)

	resp, err := s.SetScripts(context.Background(), &metadatapb.SetScriptsRequest{
		Scripts: map[string]*cvmsgspb.CronScript{
			"223e4567-e89b-12d3-a456-426655440000": s1,
			"223e4567-e89b-12d3-a456-426655440001": s2,
		},
	})
	require.Nil(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &metadatapb.SetScriptsResponse{}, resp)
}
