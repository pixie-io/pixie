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

package util

import "math/rand"

const (
	randAllocSize = 128 * 1024 * 1024
)

var preAllocRand = make([]byte, randAllocSize)

func init() {
	for i := 0; i < len(preAllocRand); i++ {
		// ASCII: <space> (32) to ~ (126)
		preAllocRand[i] = byte(rand.Intn(94) + 32)
	}
}

// RandPrintable generates a random string of printable characters (ASCII Range 32 - 126).
// Note that these are just random enough to make them incompressible.
// We pre-allocate a large buffer of random data and just read from that buffer at random
// offsets. If you request enough data, there will certainly be large repeating sequences.
func RandPrintable(size int) []byte {
	out := make([]byte, size)
	w := 0
	for w < size {
		start := rand.Intn(randAllocSize)
		w += copy(out[w:], preAllocRand[start:])
	}
	return out
}
