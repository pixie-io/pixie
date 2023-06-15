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

package pebbledb

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestKeyUpperBound_Simple(t *testing.T) {
	in := "prefix"
	upperBound := KeyUpperBound([]byte(in))
	assert.Equal(t, "prefiy", string(upperBound))
}

func TestKeyUpperBound_Empty(t *testing.T) {
	in := ""
	upperBound := KeyUpperBound([]byte(in))
	assert.Nil(t, upperBound)
}

func TestKeyUpperBound_Nil(t *testing.T) {
	upperBound := KeyUpperBound(nil)
	assert.Nil(t, upperBound)
}

func TestKeyUpperBound_AllMax(t *testing.T) {
	in := []byte{255, 255, 255, 255}
	upperBound := KeyUpperBound([]byte(in))
	assert.Nil(t, upperBound)
}

func TestKeyUpperBound_LastMax(t *testing.T) {
	in := []byte{40, 41, 42, 255}
	upperBound := KeyUpperBound([]byte(in))
	assert.Equal(t, []byte{40, 41, 43}, upperBound)
}

func Fuzz_KeyUpperBound(f *testing.F) {
	f.Add([]byte("test"))
	f.Add([]byte{40, 41, 42, 255})
	f.Add([]byte{40, 41, 255, 255})
	f.Add([]byte{255, 255})

	f.Fuzz(func(t *testing.T, input []byte) {
		bound := KeyUpperBound(input)

		if len(input) == 0 {
			assert.Nil(t, bound)
			return
		}

		// Find the last index that's not 0xff (i.e. max byte)
		i := len(input) - 1
		for i >= 0 {
			if input[i] != 255 {
				break
			}
			i--
		}

		// All bytes are max
		if i < 0 {
			assert.Nil(t, bound)
			return
		}

		assert.Equal(t, i+1, len(bound))
		assert.Equal(t, input[:i], bound[:i])
		if i < len(input) {
			assert.Equal(t, input[i]+1, bound[i])
		}
	})
}
