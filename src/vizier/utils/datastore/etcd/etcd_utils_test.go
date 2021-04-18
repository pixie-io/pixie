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

package etcd

import (
	"fmt"
	"math"
	"math/rand"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	clientv3 "go.etcd.io/etcd/client/v3"
)

func randString(size int) string {
	largeVal := make([]byte, size)
	for i := 0; i < size; i++ {
		largeVal[i] = byte(rand.Intn(26) + 97)
	}
	return string(largeVal)
}

func TestBatching_NumOps(t *testing.T) {
	numOps := 300
	ops := make([]clientv3.Op, numOps)
	for i := 0; i < numOps; i++ {
		ops[i] = clientv3.OpGet(fmt.Sprint(i))
	}

	batches, err := createBatches(ops)
	require.NoError(t, err)
	numExpectedBatches := int(math.Ceil(float64(numOps) / float64(maxTxnOps)))
	assert.Len(t, batches, numExpectedBatches)
	for i := 0; i < numExpectedBatches; i++ {
		expectedOpsInBatch := maxTxnOps
		if i == numExpectedBatches-1 {
			expectedOpsInBatch = numOps % maxTxnOps
		}
		assert.Len(t, batches[i], expectedOpsInBatch)
	}
}

func TestBatching_OpSize(t *testing.T) {
	largeStr := randString(16 * 1024)

	numOps := 200
	ops := make([]clientv3.Op, numOps)
	for i := 0; i < numOps; i++ {
		ops[i] = clientv3.OpPut(fmt.Sprint(i), largeStr)
	}

	batchByteSize := func(batch []clientv3.Op) int {
		batchBytes := 0
		for _, op := range batch {
			batchBytes += len(op.ValueBytes())
		}
		return batchBytes
	}

	batches, err := createBatches(ops)
	require.NoError(t, err)
	for i := 0; i < len(batches); i++ {
		assert.LessOrEqual(t, len(batches[i]), maxTxnOps)
		assert.LessOrEqual(t, batchByteSize(batches[i]), maxNumBytes)
	}
}

func TestBatching_OpTooBig(t *testing.T) {
	oversizedStr := randString(1024*1024 + 1)

	numOps := 2
	ops := make([]clientv3.Op, numOps)
	for i := 0; i < numOps; i++ {
		ops[i] = clientv3.OpPut(fmt.Sprint(i), oversizedStr)
	}

	_, err := createBatches(ops)
	assert.EqualError(t, err, "clientv3 operation bytes larger than max request bytes")
}
