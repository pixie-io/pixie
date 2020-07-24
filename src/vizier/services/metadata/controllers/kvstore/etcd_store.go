package kvstore

import (
	"context"
	"errors"

	v3 "github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/mvcc/mvccpb"
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

// GetAll gets the values for all of the given keys.
func (e *EtcdStore) GetAll(keys []string) ([][]byte, error) {
	ops := make([]v3.Op, len(keys))
	for i, k := range keys {
		ops[i] = v3.OpGet(k)
	}

	resp, err := etcdutils.BatchOps(e.client, ops)
	if err != nil {
		return nil, err
	}

	vals := make([][]byte, len(keys))
	for i, r := range resp {
		rRange := r.GetResponseRange().Kvs
		if len(rRange) > 0 {
			vals[i] = r.GetResponseRange().Kvs[0].Value
			continue
		}
		vals[i] = nil

	}

	return vals, nil
}

// SetAll performs a put on all of the given keys and values in etcd.
func (e *EtcdStore) SetAll(keysValues []TTLKeyValue) error {
	ops := make([]v3.Op, len(keysValues))
	for i, kv := range keysValues {
		leaseID := v3.NoLease
		if kv.Expire && kv.TTL > 0 {
			resp, err := e.client.Grant(context.TODO(), kv.TTL)
			if err != nil {
				return errors.New("Could not get grant for lease")
			}
			leaseID = resp.ID
			ops[i] = v3.OpPut(kv.Key, string(kv.Value), v3.WithLease(leaseID))
		} else if kv.Expire && kv.TTL <= 0 {
			ops[i] = v3.OpDelete(kv.Key)
		} else {
			ops[i] = v3.OpPut(kv.Key, string(kv.Value), v3.WithLease(leaseID))
		}
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

// DeleteWithPrefix deletes all keys and values with the given prefix.
func (e *EtcdStore) DeleteWithPrefix(prefix string) error {
	_, err := e.client.Delete(context.Background(), prefix, v3.WithPrefix())
	return err
}

// GetWithRange gets all the keys and values within the given range.
func (e *EtcdStore) GetWithRange(from string, to string) ([]string, [][]byte, error) {
	resp, err := e.client.Get(context.Background(), from, v3.WithRange(to))
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

// WatchKeyEvents watches any events that occur to the keys starting with the given prefix.
func (e *EtcdStore) WatchKeyEvents(prefix string) (chan KeyEvent, chan bool) {
	eventCh := make(chan KeyEvent, 1000)

	ch := e.client.Watch(context.Background(), prefix, v3.WithPrefix())
	quitCh := make(chan bool)

	go func() {
		defer close(eventCh)
		defer close(quitCh)
		for {
			select {
			case <-quitCh:
				return
			case resp := <-ch:
				for _, ev := range resp.Events {
					eType := EventTypeDelete
					if ev.Type == mvccpb.PUT {
						eType = EventTypePut
					}
					eventCh <- KeyEvent{EventType: eType, Key: string(ev.Kv.Key)}
				}
			}
		}
	}()

	return eventCh, quitCh
}
