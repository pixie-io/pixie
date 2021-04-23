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

package testutils_test

import (
	"context"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/profile/controller/idmanager"
	"px.dev/pixie/src/cloud/shared/idprovider/testutils"
)

func TestCreateKratos(t *testing.T) {
	server, err := testutils.NewKratosServer()

	require.NoError(t, err)

	defer server.CleanUp()

	client, closeServer, err := server.Serve()
	defer closeServer()
	require.NoError(t, err)

	// Simple test that should pass if the client is setup properly.
	email := "bobloblaw@lawblog.com"
	idResp, err := client.CreateInviteLink(
		context.Background(),
		&idmanager.CreateInviteLinkRequest{
			Email:    email,
			PLUserID: email,
			PLOrgID:  "lawblow.com",
		},
	)
	require.NoError(t, err)
	assert.Equal(t, idResp.Email, email)
}
