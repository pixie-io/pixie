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

package scripts

import (
	"crypto/sha256"
	"encoding/json"
	"errors"

	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/shared/cvmsgspb"
)

func xorByteArray(a, b []byte) ([]byte, error) {
	if len(a) != len(b) {
		return nil, errors.New("Byte arrays must be the same length")
	}

	c := make([]byte, len(a))
	for i := range a {
		c[i] = a[i] ^ b[i]
	}
	return c, nil
}

// ChecksumFromScriptMap gets a checksum representing the registered cron scripts for a Vizier.
func ChecksumFromScriptMap(scripts map[string]*cvmsgspb.CronScript) (string, error) {
	h := sha256.New()
	checksum := h.Sum(nil)

	for _, s := range scripts {
		scriptStr, err := json.Marshal(s)
		if err != nil {
			log.WithError(err).Error("Failed to get checksum")
			return "", err
		}
		h2 := sha256.New()
		h2.Write([]byte(scriptStr))

		hash := h2.Sum(nil)
		checksum, err = xorByteArray(checksum, hash)
		if err != nil {
			return "", err
		}
	}
	return string(checksum), nil
}
