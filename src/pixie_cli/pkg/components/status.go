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

package components

import (
	"strings"

	"github.com/fatih/color"
)

var (
	statusOK  = "\u2714"
	statusErr = "\u2715"
)

func computePadding(s string, pad int) (int, int) {
	var padS, padE int
	pad -= len([]rune(s))
	if pad < 0 {
		pad = 0
	}

	padS = int(pad / 2)
	padE = padS
	if pad%2 != 0 {
		padE = padS + 1
	}
	return padS, padE
}

// StatusOK prints out the default OK symbol, center padded to the size specified.
func StatusOK(pad int) string {
	padS, padE := computePadding(statusOK, pad)
	return strings.Repeat(" ", padS) + color.GreenString(statusOK) + strings.Repeat(" ", padE)
}

// StatusErr prints out the default Err symbol, center padded to the size specified.
func StatusErr(pad int) string {
	padS, padE := computePadding(statusErr, pad)
	return strings.Repeat(" ", padS) + color.RedString(statusErr) + strings.Repeat(" ", padE)
}
