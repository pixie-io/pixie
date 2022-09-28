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
	"bytes"
	"errors"
	"fmt"
	"time"

	"px.dev/pixie/src/shared/services/msgbus"
)

func receiveExpectedUpdates(c <-chan msgbus.Msg, data [][]byte) error {
	// On some investigations, found that messages are sent < 500us, chose a
	// timeout that was 100x in case of any unexpected interruptions.
	timeout := 50 * time.Millisecond

	// We do not early return any errors, otherwise we won't consume all messages
	// sent and risk race conditions in tests.
	// If no errors are reached, `err` will be nil.
	var err error

	curr := 0
	for {
		select {
		case m := <-c:
			if curr >= len(data) {
				err = fmt.Errorf("Unexpected message: %s", string(m.Data()))
			} else if !bytes.Equal(data[curr], m.Data()) {
				err = fmt.Errorf("Data doesn't match on update %d", curr)
			}
			curr++
		case <-time.After(timeout):
			if curr < len(data) {
				return errors.New("Timed out waiting for messages on subscription")
			}
			return err
		}
	}
}
