package etcd_test

import (
	"context"
	"fmt"
	"testing"

	"github.com/coreos/etcd/clientv3"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
)

func testBatchSize(t *testing.T, size int) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

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
