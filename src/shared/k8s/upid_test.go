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

package k8s_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/k8s"
	types "px.dev/pixie/src/shared/types/gotypes"
)

func TestUPID(t *testing.T) {
	upid := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	assert.Equal(t, uint32(123), k8s.ASIDFromUPID(upid))
	assert.Equal(t, uint32(567), k8s.PIDFromUPID(upid))
	assert.Equal(t, uint64(89101), k8s.StartTSFromUPID(upid))
	assert.Equal(t, "123:567:89101", k8s.StringFromUPID(upid))
}

func TestUPIDFromString(t *testing.T) {
	upid := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	upidString := k8s.StringFromUPID(upid)

	assert.Equal(t, "123:567:89101", upidString)

	upidObj, err := k8s.UPIDFromString(upidString)
	require.NoError(t, err)
	assert.Equal(t, *upid, upidObj)
}
