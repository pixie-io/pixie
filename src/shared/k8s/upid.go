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

package k8s

import (
	"fmt"
	"strconv"
	"strings"

	log "github.com/sirupsen/logrus"

	types "px.dev/pixie/src/shared/types/gotypes"
)

// ASIDFromUPID gets the ASID from the UPID.
func ASIDFromUPID(u *types.UInt128) uint32 {
	return uint32(u.High >> 32)
}

// PIDFromUPID gets the PID from the UPID.
func PIDFromUPID(u *types.UInt128) uint32 {
	return uint32(u.High)
}

// StartTSFromUPID gets the start timestamp from the UPID.
func StartTSFromUPID(u *types.UInt128) uint64 {
	return uint64(u.Low)
}

// StringFromUPID gets a string from the UPID.
func StringFromUPID(u *types.UInt128) string {
	return fmt.Sprintf("%d:%d:%d", ASIDFromUPID(u), PIDFromUPID(u), StartTSFromUPID(u))
}

// UPIDFromString converts a upid in a string into a UINT128. Returns an error if string is malformed.
func UPIDFromString(upidString string) (types.UInt128, error) {
	upid := types.UInt128{}
	splitUpid := strings.Split(upidString, ":")
	if len(splitUpid) != 3 {
		return upid, fmt.Errorf("UPID string malformed: '%s'", upidString)
	}
	asid, err := strconv.Atoi(splitUpid[0])
	if err != nil {
		log.WithError(err).Error("Could not unmarshal upid string.")
		return upid, err
	}
	pid, err := strconv.Atoi(splitUpid[1])
	if err != nil {
		log.WithError(err).Error("Could not unmarshal upid string.")
		return upid, err
	}
	ts, err := strconv.Atoi(splitUpid[2])
	if err != nil {
		log.WithError(err).Error("Could not unmarshal upid string.")
		return upid, err
	}
	upid.High = uint64(asid)<<32 + uint64(pid)
	upid.Low = uint64(ts)

	return upid, nil
}
