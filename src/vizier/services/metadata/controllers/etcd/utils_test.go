package etcd_test

import (
	"context"
	"crypto/rand"
	"fmt"
	"testing"

	"github.com/coreos/etcd/clientv3"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
)

func testBatchSize(t *testing.T, size int) {
	clearEtcd(t)

	// Add items into etcd that we can get.
	i := 0
	ops := make([]clientv3.Op, size)
	for i < size {
		_, err := etcdClient.Put(context.Background(), "/"+fmt.Sprint(i), fmt.Sprint(i))
		if err != nil {
			t.Fatal("Unable to add agentData to etcd.")
		}
		ops[i] = clientv3.OpGet("/" + fmt.Sprint(i))
		i++
	}

	resp, err := etcd.BatchOps(etcdClient, ops)
	assert.Equal(t, size, len(resp))
	assert.Nil(t, err)
	i = 0
	for i < size {
		assert.Equal(t, fmt.Sprint(i), string(resp[i].GetResponseRange().Kvs[0].Value))
		i++
	}
}

func TestSmallerBatch(t *testing.T) {
	testBatchSize(t, 64)
}

func TestEqualToOneBatch(t *testing.T) {
	testBatchSize(t, 128)
}

func TestLargerThanOneBatch(t *testing.T) {
	testBatchSize(t, 150)
}

func TestTwoBatches(t *testing.T) {
	testBatchSize(t, 256)
}

func testBatchBytesSize(t *testing.T, size int, valueSize int) {
	clearEtcd(t)

	i := 0
	keyValues := make(map[int][]byte)
	ops := make([]clientv3.Op, size)
	for i < size {
		randValue := make([]byte, valueSize)
		_, err := rand.Read(randValue)
		assert.Nil(t, err)
		keyValues[i] = randValue
		ops[i] = clientv3.OpPut("/"+fmt.Sprint(i), string(randValue))
		i++
	}

	resp, err := etcd.BatchOps(etcdClient, ops)
	assert.Equal(t, size, len(resp))
	assert.Nil(t, err)

	i = 0
	for i < size {
		val, err := etcdClient.Get(context.Background(), "/"+fmt.Sprint(i))
		assert.Nil(t, err)
		assert.Equal(t, 1, len(val.Kvs))
		assert.Equal(t, keyValues[i], val.Kvs[0].Value)
		i++
	}
}

func TestSmallerBytes(t *testing.T) {
	testBatchBytesSize(t, 127, 1)
}

func TestLargerBytes(t *testing.T) {
	testBatchBytesSize(t, 16, 131072)
}

func TestTwoByteBatches(t *testing.T) {
	testBatchBytesSize(t, 3, 1048576)
}
