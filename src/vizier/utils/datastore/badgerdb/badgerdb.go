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

package badgerdb

import (
	"bytes"
	"time"

	"github.com/dgraph-io/badger/v3"
)

// DataStore wraps a badgerdb datastore.
type DataStore struct {
	db *badger.DB
}

// New creates a new badgerdb for use as a KVStore.
func New(db *badger.DB) *DataStore {
	wrap := &DataStore{
		db: db,
	}

	return wrap
}

// Get gets the value for the given key from the datastore.
func (w *DataStore) Get(key string) ([]byte, error) {
	txn := w.db.NewTransaction(false)
	defer txn.Discard()

	item, err := txn.Get([]byte(key))
	if err == badger.ErrKeyNotFound {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}

	return item.ValueCopy(nil)
}

// GetWithRange gets all keys and values within the given range.
// Treats this as [from, to) i.e. includes the key from, but excludes the key to.
func (w *DataStore) GetWithRange(from string, to string) ([]string, [][]byte, error) {
	txn := w.db.NewTransaction(false)
	defer txn.Discard()

	it := txn.NewIterator(badger.DefaultIteratorOptions)
	defer it.Close()

	f := []byte(from)
	t := []byte(to)

	var keys []string
	var values [][]byte

	for it.Seek(f); it.Valid(); it.Next() {
		item := it.Item()
		if bytes.Compare(item.Key(), t) >= 0 {
			break
		}
		v, err := item.ValueCopy(nil)
		if err != nil {
			return nil, nil, err
		}
		keys = append(keys, string(item.Key()))
		values = append(values, v)
	}
	return keys, values, nil
}

// GetWithPrefix gets all keys and values with the given prefix.
func (w *DataStore) GetWithPrefix(prefix string) ([]string, [][]byte, error) {
	txn := w.db.NewTransaction(false)
	defer txn.Discard()

	pre := []byte(prefix)

	opts := badger.DefaultIteratorOptions
	opts.Prefix = pre

	it := txn.NewIterator(opts)
	defer it.Close()

	var keys []string
	var values [][]byte

	for it.Seek(pre); it.ValidForPrefix(pre); it.Next() {
		item := it.Item()
		v, err := item.ValueCopy(nil)
		if err != nil {
			return nil, nil, err
		}
		keys = append(keys, string(item.Key()))
		values = append(values, v)
	}
	return keys, values, nil
}

// Set puts the given key and value in the datastore.
func (w *DataStore) Set(key string, value string) error {
	txn := w.db.NewTransaction(true)
	defer txn.Discard()

	err := txn.Set([]byte(key), []byte(value))
	if err != nil {
		return err
	}

	return txn.Commit()
}

// SetWithTTL puts the given key and value into the datastore with a TTL.
// Once the TTL expires the datastore is expected to delete the given key and value.
func (w *DataStore) SetWithTTL(key string, value string, ttl time.Duration) error {
	txn := w.db.NewTransaction(true)
	defer txn.Discard()

	e := badger.NewEntry([]byte(key), []byte(value)).WithTTL(ttl)
	err := txn.SetEntry(e)
	if err != nil {
		return err
	}

	return txn.Commit()
}

// Delete deletes the value for the given key from the datastore.
func (w *DataStore) Delete(key string) error {
	txn := w.db.NewTransaction(true)
	defer txn.Discard()

	err := txn.Delete([]byte(key))
	if err != nil {
		return err
	}

	return txn.Commit()
}

// DeleteAll deletes all of the given keys and corresponding values in the datastore if they exist.
func (w *DataStore) DeleteAll(keys []string) error {
	wb := w.db.NewWriteBatch()
	defer wb.Cancel()

	for _, key := range keys {
		err := wb.Delete([]byte(key))
		if err != nil {
			return err
		}
	}

	return wb.Flush()
}

// DeleteWithPrefix deletes all keys and values with the given prefix.
func (w *DataStore) DeleteWithPrefix(prefix string) error {
	txn := w.db.NewTransaction(false)
	defer txn.Discard()

	opts := badger.DefaultIteratorOptions
	opts.PrefetchValues = false

	it := txn.NewIterator(opts)
	defer it.Close()

	wb := w.db.NewWriteBatch()
	defer wb.Cancel()

	for it.Seek([]byte(prefix)); it.ValidForPrefix([]byte(prefix)); it.Next() {
		item := it.Item()
		err := wb.Delete(item.KeyCopy(nil))
		if err != nil {
			return err
		}
	}
	return wb.Flush()
}

// Close stops the TTL watcher, and closes the underlying datastore.
// All other operations will fail after calling Close.
func (w *DataStore) Close() error {
	return w.db.Close()
}
