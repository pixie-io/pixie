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

package msgbus_test

import (
	"testing"

	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/utils/testingutils"
)

func TestMustConnectNATS(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	viper.Set("nats_url", nc.ConnectedUrl())
	viper.Set("disable_ssl", true)

	sub := "sub"
	msg := []byte("test")
	ch := make(chan *nats.Msg)
	_, err := nc.ChanSubscribe(sub, ch)
	require.NoError(t, err)
	err = nc.Publish(sub, msg)
	require.NoError(t, err)
	natsMsg := <-ch
	assert.Equal(t, natsMsg.Data, msg)
}
