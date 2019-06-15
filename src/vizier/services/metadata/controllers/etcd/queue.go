package etcd

import (
	"context"
	"fmt"
	"time"

	v3 "github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/contrib/recipes"
	spb "github.com/coreos/etcd/mvcc/mvccpb"
)

// Modified from https://github.com/etcd-io/etcd/blob/34bd797e6754911ee540e8c87f708f88ffe89f37/clientv3/concurrency/key.go
// newUniqueKV puts a new key-value into etcd with the current timestamp as the key's suffix.
func newUniqueKV(kv v3.KV, prefix string, val string) error {
	for {
		newKey := fmt.Sprintf("%s/%v", prefix, time.Now().UnixNano())
		_, err := putNewKV(kv, newKey, val, v3.NoLease)

		if err != recipe.ErrKeyExists {
			return err
		}
		return nil
	}
}

// Copied from https://github.com/etcd-io/etcd/blob/34bd797e6754911ee540e8c87f708f88ffe89f37/clientv3/concurrency/key.go
// putNewKV attempts to create the given key, only succeeding if the key doesn't already exist.
func putNewKV(kv v3.KV, key, val string, leaseID v3.LeaseID) (int64, error) {
	cmp := v3.Compare(v3.Version(key), "=", 0)
	req := v3.OpPut(key, val, v3.WithLease(leaseID))
	txnresp, err := kv.Txn(context.TODO()).If(cmp).Then(req).Commit()
	if err != nil {
		return 0, err
	}
	if !txnresp.Succeeded {
		return 0, recipe.ErrKeyExists
	}
	return txnresp.Header.Revision, nil
}

// Copied from https://github.com/etcd-io/etcd/blob/34bd797e6754911ee540e8c87f708f88ffe89f37/contrib/recipes/client.go
// deleteRevKey deletes a key by revision, returning false if the key is missing.
func deleteRevKey(kv v3.KV, key string, rev int64) (bool, error) {
	cmp := v3.Compare(v3.ModRevision(key), "=", rev)
	req := v3.OpDelete(key)
	txnresp, err := kv.Txn(context.TODO()).If(cmp).Then(req).Commit()
	if err != nil {
		return false, err
	} else if !txnresp.Succeeded {
		return false, nil
	}
	return true, nil
}

// Copied from https://github.com/etcd-io/etcd/blob/34bd797e6754911ee540e8c87f708f88ffe89f37/contrib/recipes/client.go
// claimFirstKey deletes the first key from the given list of keys and values.
func claimFirstKey(kv v3.KV, kvs []*spb.KeyValue) (*spb.KeyValue, error) {
	for _, k := range kvs {
		ok, err := deleteRevKey(kv, string(k.Key), k.ModRevision)
		if err != nil {
			return nil, err
		} else if ok {
			return k, nil
		}
	}
	return nil, nil
}

// Queue implements a multi-reader, multi-writer distributed queue.
type Queue struct {
	client *v3.Client
	ctx    context.Context

	keyPrefix string
}

// NewQueue creates a new queue.
func NewQueue(client *v3.Client, keyPrefix string) *Queue {
	return &Queue{client, context.TODO(), keyPrefix + "/queue"}
}

// Enqueue adds a new value to the queue.
// Modified from https://github.com/etcd-io/etcd/blob/34bd797e6754911ee540e8c87f708f88ffe89f37/contrib/recipes/queue.go
func (q *Queue) Enqueue(val string) error {
	err := newUniqueKV(q.client, q.keyPrefix, val)
	return err
}

// Dequeue returns the first value in the queue. If there are no items in the queue, it will return an empty string.
// Modified from https://github.com/etcd-io/etcd/blob/34bd797e6754911ee540e8c87f708f88ffe89f37/contrib/recipes/queue.go
func (q *Queue) Dequeue() (string, error) {
	resp, err := q.client.Get(q.ctx, q.keyPrefix, v3.WithFirstRev()...)
	if err != nil {
		return "", err
	}

	kv, err := claimFirstKey(q.client, resp.Kvs)
	if err != nil {
		return "", err
	} else if kv != nil {
		return string(kv.Value), nil
	} else if resp.More {
		// missed some items, retry to read in more
		return q.Dequeue()
	}

	return "", nil
}

// DequeueAll returns all items currently in the queue.
func (q *Queue) DequeueAll() (*[]string, error) {
	resp, err := q.client.Get(q.ctx, q.keyPrefix, v3.WithPrefix())
	if err != nil {
		return nil, err
	}

	vals := make([]string, len(resp.Kvs))
	for i, kv := range resp.Kvs {
		vals[i] = string(kv.Value)
	}

	if len(resp.Kvs) > 0 {
		lastKey := string(resp.Kvs[len(resp.Kvs)-1].Key)
		// Deletes [firstKey, lastKey).
		_, err = q.client.Delete(q.ctx, q.keyPrefix, v3.WithRange(lastKey))
		if err != nil {
			return nil, err
		}
		// Delete lastKey.
		_, err = q.client.Delete(q.ctx, lastKey)
		if err != nil {
			return nil, err
		}
	}

	return &vals, nil
}
