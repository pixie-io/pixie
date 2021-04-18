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
	"context"
	"sync"
	"time"

	"go.etcd.io/etcd/api/v3/mvccpb"
	clientv3 "go.etcd.io/etcd/client/v3"
)

// DataStore wraps a clientv3 datastore.
type DataStore struct {
	client *clientv3.Client
	once   sync.Once
}

// New creates a new clientv3 for use as a KVStore.
func New(client *clientv3.Client) *DataStore {
	return &DataStore{client: client}
}

// Set puts the given key and value in the datastore.
func (w *DataStore) Set(key string, value string) error {
	_, err := w.client.Put(context.Background(), key, value)
	return err
}

// SetWithTTL puts the given key and value into the datastore with a TTL.
// Once the TTL expires the datastore is expected to delete the given key and value.
func (w *DataStore) SetWithTTL(key string, value string, ttl time.Duration) error {
	resp, err := w.client.Grant(context.Background(), int64(ttl.Seconds()))
	if err != nil {
		return err
	}
	leaseID := resp.ID

	_, err = w.client.Put(context.Background(), key, value, clientv3.WithLease(leaseID))
	return err
}

// Get gets the value for the given key from the datastore.
func (w *DataStore) Get(key string) ([]byte, error) {
	resp, err := w.client.Get(context.Background(), key, clientv3.WithSerializable())
	if err != nil {
		return nil, err
	}
	if len(resp.Kvs) == 0 {
		return nil, nil
	}
	return resp.Kvs[0].Value, nil
}

func kvsToSlices(kvs []*mvccpb.KeyValue) ([]string, [][]byte, error) {
	if len(kvs) == 0 {
		return nil, nil, nil
	}

	keys := make([]string, len(kvs))
	values := make([][]byte, len(kvs))

	for i, kv := range kvs {
		keys[i] = string(kv.Key)
		values[i] = kv.Value
	}

	return keys, values, nil
}

// GetWithRange gets all keys and values within the given range.
// Treats this as [from, to) i.e. includes the key from, but excludes the key to.
func (w *DataStore) GetWithRange(from string, to string) ([]string, [][]byte, error) {
	resp, err := w.client.Get(context.Background(), from, clientv3.WithRange(to), clientv3.WithSerializable())
	if err != nil {
		return nil, nil, err
	}

	return kvsToSlices(resp.Kvs)
}

// GetWithPrefix gets all keys and values with the given prefix.
func (w *DataStore) GetWithPrefix(prefix string) ([]string, [][]byte, error) {
	resp, err := w.client.Get(context.Background(), prefix, clientv3.WithPrefix(), clientv3.WithSerializable())
	if err != nil {
		return nil, nil, err
	}

	return kvsToSlices(resp.Kvs)
}

// Delete deletes the value for the given key from the datastore.
func (w *DataStore) Delete(key string) error {
	_, err := w.client.Delete(context.Background(), key)
	return err
}

// DeleteAll deletes all of the given keys and corresponding values in the datastore if they exist.
func (w *DataStore) DeleteAll(keys []string) error {
	ops := make([]clientv3.Op, len(keys))
	for i, k := range keys {
		ops[i] = clientv3.OpDelete(k)
	}

	_, err := batchOps(context.Background(), w.client, ops)
	return err
}

// DeleteWithPrefix deletes all keys and values with the given prefix.
func (w *DataStore) DeleteWithPrefix(prefix string) error {
	_, err := w.client.Delete(context.Background(), prefix, clientv3.WithPrefix())
	return err
}

// Close closes the underlying datastore.
// All other operations will fail after calling Close.
func (w *DataStore) Close() error {
	var err error
	w.once.Do(func() {
		err = w.client.Close()
	})
	return err
}
