package kvstore

import (
	"context"
	"errors"
	"time"

	v3 "github.com/coreos/etcd/clientv3"
	etcdutils "pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
)

// EtcdStore is a wrapper around the etcd client which implements the KeyValueStore interface.
type EtcdStore struct {
	client *v3.Client
}

// NewEtcdStore creates a new etcd store.
func NewEtcdStore(client *v3.Client) *EtcdStore {
	return &EtcdStore{client}
}

// Get gets the given key from etcd.
func (e *EtcdStore) Get(key string) ([]byte, error) {
	resp, err := e.client.Get(context.Background(), key)
	if err != nil {
		return nil, err
	}
	if len(resp.Kvs) == 0 {
		return nil, nil
	}
	return resp.Kvs[0].Value, nil
}

// SetAll performs a put on all of the given keys and values in etcd.
func (e *EtcdStore) SetAll(keysValues *map[string]Entry) error {
	ops := make([]v3.Op, len(*keysValues))
	i := 0
	for k, v := range *keysValues {
		leaseID := v3.NoLease
		if !v.ExpiresAt.IsZero() {
			ttl := v.ExpiresAt.Sub(time.Now()).Seconds()
			resp, err := e.client.Grant(context.TODO(), int64(ttl))
			if err != nil {
				return errors.New("Could not get grant for lease")
			}
			leaseID = resp.ID
		}
		ops[i] = v3.OpPut(k, string(v.Value), v3.WithLease(leaseID))
		i++
	}

	_, err := etcdutils.BatchOps(e.client, ops)
	return err
}

// GetWithPrefix gets all keys and values starting with the given prefix.
func (e *EtcdStore) GetWithPrefix(prefix string) ([]string, [][]byte, error) {
	resp, err := e.client.Get(context.Background(), prefix, v3.WithPrefix())
	if err != nil {
		return nil, nil, err
	}

	keys := make([]string, len(resp.Kvs))
	values := make([][]byte, len(resp.Kvs))

	for i, kv := range resp.Kvs {
		keys[i] = string(kv.Key)
		values[i] = kv.Value
	}

	return keys, values, nil
}
