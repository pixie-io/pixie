package etcd

import (
	"fmt"
	"math"
	"math/rand"
	"testing"

	etcd "github.com/coreos/etcd/clientv3"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
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
	ops := make([]etcd.Op, numOps)
	for i := 0; i < numOps; i++ {
		ops[i] = etcd.OpGet(fmt.Sprint(i))
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
	ops := make([]etcd.Op, numOps)
	for i := 0; i < numOps; i++ {
		ops[i] = etcd.OpPut(fmt.Sprint(i), largeStr)
	}

	batchByteSize := func(batch []etcd.Op) int {
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
	ops := make([]etcd.Op, numOps)
	for i := 0; i < numOps; i++ {
		ops[i] = etcd.OpPut(fmt.Sprint(i), oversizedStr)
	}

	_, err := createBatches(ops)
	assert.EqualError(t, err, "etcd operation bytes larger than max request bytes")
}
