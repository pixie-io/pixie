package etcd_test

import (
	"context"
	"os"
	"testing"

	"github.com/coreos/etcd/clientv3"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
)

var etcdClient *clientv3.Client

func clearEtcd(t *testing.T) {
	_, err := etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}
}

func TestEnqueue(t *testing.T) {
	clearEtcd(t)

	q := etcd.NewQueue(etcdClient, "test")

	err := q.Enqueue("abcd")
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	err = q.Enqueue("efgh")
	assert.Nil(t, err)

	// First revision should still be key with value abcd.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	// Last revision should be key with value efgh.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithLastRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "efgh", string(resp.Kvs[0].Value))
}

func TestDequeueMultiple(t *testing.T) {
	clearEtcd(t)

	q := etcd.NewQueue(etcdClient, "test")

	err := q.Enqueue("abcd")
	assert.Nil(t, err)

	err = q.Enqueue("efgh")
	assert.Nil(t, err)

	err = q.Enqueue("ijkl")
	assert.Nil(t, err)

	resp, err := q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "abcd", resp)

	resp, err = q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "efgh", resp)

	resp, err = q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "ijkl", resp)

	resp, err = q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "", resp)
}

func TestDequeueEmpty(t *testing.T) {
	clearEtcd(t)

	q := etcd.NewQueue(etcdClient, "test")

	resp, err := q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "", resp)
}

func TestDequeueAll(t *testing.T) {
	clearEtcd(t)

	q := etcd.NewQueue(etcdClient, "test")

	err := q.Enqueue("abcd")
	assert.Nil(t, err)

	err = q.Enqueue("efgh")
	assert.Nil(t, err)

	err = q.Enqueue("ijkl")
	assert.Nil(t, err)

	q1 := etcd.NewQueue(etcdClient, "test2")

	err = q1.Enqueue("abcd")
	assert.Nil(t, err)

	resp, err := q.DequeueAll()

	assert.Nil(t, err)
	assert.Equal(t, 3, len(*resp))
	assert.Equal(t, "abcd", (*resp)[0])
	assert.Equal(t, "efgh", (*resp)[1])
	assert.Equal(t, "ijkl", (*resp)[2])

	resp, err = q.DequeueAll()

	assert.Nil(t, err)
	assert.Equal(t, 0, len(*resp))

	// Check that unrelated queue is unaffected.
	resp1, err := q1.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "abcd", resp1)
}

func TestEnqueueAtFront(t *testing.T) {
	clearEtcd(t)

	q := etcd.NewQueue(etcdClient, "test")

	err := q.Enqueue("abcd")
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	err = q.Enqueue("efgh")
	assert.Nil(t, err)

	// First revision should still be key with value abcd.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	// Last revision should be key with value efgh.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithLastRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "efgh", string(resp.Kvs[0].Value))

	err = q.EnqueueAtFront("ijkl")
	if err != nil {
		t.Fatal("Failed to add item to front of queue.")
	}

	// First revision should now be key with value ijkl.
	dequeueResp, err := q.Dequeue()
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, "ijkl", dequeueResp)
}

func TestEnqueueAll(t *testing.T) {
	clearEtcd(t)

	q := etcd.NewQueue(etcdClient, "test")

	err := q.Enqueue("abcd")
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	err = q.Enqueue("efgh")
	assert.Nil(t, err)

	// First revision should still be key with value abcd.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	// Last revision should be key with value efgh.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithLastRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "efgh", string(resp.Kvs[0].Value))

	err = q.EnqueueAll([]string{"1", "2", "3"})
	assert.Nil(t, err)

	dequeueResp, err := q.Dequeue()
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, "abcd", dequeueResp)

	dequeueResp, err = q.Dequeue()
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, "efgh", dequeueResp)

	dequeueResp, err = q.Dequeue()
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, "1", dequeueResp)

	dequeueResp, err = q.Dequeue()
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, "2", dequeueResp)

	dequeueResp, err = q.Dequeue()
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, "3", dequeueResp)
}

func TestEnqueueAllDequeueAll(t *testing.T) {
	clearEtcd(t)

	q := etcd.NewQueue(etcdClient, "test")

	err := q.Enqueue("abcd")
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	err = q.Enqueue("efgh")
	assert.Nil(t, err)

	// First revision should still be key with value abcd.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	// Last revision should be key with value efgh.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithLastRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "efgh", string(resp.Kvs[0].Value))

	err = q.EnqueueAll([]string{"1", "2", "3"})
	assert.Nil(t, err)

	dequeueResp, err := q.DequeueAll()

	assert.Nil(t, err)
	assert.Equal(t, 5, len(*dequeueResp))
	assert.Equal(t, "abcd", (*dequeueResp)[0])
	assert.Equal(t, "efgh", (*dequeueResp)[1])
	assert.Equal(t, "1", (*dequeueResp)[2])
	assert.Equal(t, "2", (*dequeueResp)[3])
	assert.Equal(t, "3", (*dequeueResp)[4])
}

func TestMain(m *testing.M) {
	c, cleanup := testingutils.SetupEtcd()
	etcdClient = c
	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}
