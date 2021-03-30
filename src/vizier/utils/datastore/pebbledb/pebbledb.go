package pebbledb

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/cockroachdb/pebble"
)

const (
	ttlByKeyPrefix  = "___ttl___"
	ttlByTimePrefix = "___ttl_time___"
)

// DataStore wraps a pebbledb datastore.
type DataStore struct {
	db *pebble.DB

	done chan struct{}
	once sync.Once
}

// New creates a new pebbledb for use as a KVStore.
func New(db *pebble.DB, ttlReaperDuration time.Duration) *DataStore {
	wrap := &DataStore{
		db:   db,
		done: make(chan struct{}),
	}

	go wrap.ttlWatcher(ttlReaperDuration)

	return wrap
}

func (w *DataStore) ttlWatcher(ttlReaperDuration time.Duration) {
	ticker := time.NewTicker(ttlReaperDuration)
	defer ticker.Stop()
	for {
		select {
		case <-w.done:
			return
		case <-ticker.C:
			now := time.Now()

			from := fmt.Sprintf("%s/", ttlByTimePrefix)
			to := fmt.Sprintf("%s/%20d/", ttlByTimePrefix, now.Unix()+1)

			iter := w.db.NewIter(&pebble.IterOptions{
				LowerBound: []byte(from),
				UpperBound: []byte(to),
			})

			var deleteKeys []string

			for iter.First(); iter.Valid(); iter.Next() {
				if iter.Error() != nil {
					break
				}
				// Casting to a string causes a implicit copy, making
				// ensuring that this is valid across iterations.
				k := string(iter.Key())

				sp := strings.Split(k, "/")
				if len(sp) < 3 {
					continue
				}

				keyToDelete := sp[2]
				ttlByKey := fmt.Sprintf("%s/%s", ttlByKeyPrefix, keyToDelete)

				v, err := w.Get(ttlByKey)
				if err != nil {
					continue
				}

				var expiresAt time.Time
				err = expiresAt.UnmarshalBinary(v)
				if err != nil {
					continue
				}
				if expiresAt.Before(now) {
					deleteKeys = append(deleteKeys, ttlByKey)
					deleteKeys = append(deleteKeys, keyToDelete)
				}
			}
			w.DeleteAll(deleteKeys)
			w.db.DeleteRange([]byte(from), []byte(to), pebble.Sync)
			iter.Close()
		}
	}
}

// Set puts the given key and value in the datastore.
func (w *DataStore) Set(key string, value string) error {
	return w.db.Set([]byte(key), []byte(value), pebble.Sync)
}

// SetWithTTL puts the given key and value into the datastore with a TTL.
// Once the TTL expires the datastore is expected to delete the given key and value.
func (w *DataStore) SetWithTTL(key string, value string, ttl time.Duration) error {
	batch := w.db.NewBatch()
	expiresAt := time.Now().Add(ttl)
	encodedExpiry, err := expiresAt.MarshalBinary()
	if err != nil {
		batch.Close()
		return err
	}
	err = batch.Set([]byte(key), []byte(value), pebble.Sync)
	if err != nil {
		batch.Close()
		return err
	}

	ttlByKey := fmt.Sprintf("%s/%s", ttlByKeyPrefix, key)
	ttlByTime := fmt.Sprintf("%s/%20d/%s", ttlByTimePrefix, expiresAt.Unix(), key)

	err = batch.Set([]byte(ttlByKey), encodedExpiry, pebble.Sync)
	if err != nil {
		batch.Close()
		return err
	}
	err = batch.Set([]byte(ttlByTime), nil, pebble.Sync)
	if err != nil {
		batch.Close()
		return err
	}
	return batch.Commit(pebble.Sync)
}

// Get gets the value for the given key from the datastore.
func (w *DataStore) Get(key string) ([]byte, error) {
	v, closer, err := w.db.Get([]byte(key))
	if err == pebble.ErrNotFound {
		return nil, nil
	}
	value := make([]byte, len(v))
	copy(value, v)
	return value, closer.Close()
}

// GetWithRange gets all keys and values within the given range.
// Treats this as [from, to) i.e. includes the key from, but excludes the key to.
func (w *DataStore) GetWithRange(from string, to string) ([]string, [][]byte, error) {
	var keys []string
	var values [][]byte

	iter := w.db.NewIter(&pebble.IterOptions{
		LowerBound: []byte(from),
		UpperBound: []byte(to),
	})

	for iter.First(); iter.Valid(); iter.Next() {
		if err := iter.Error(); err != nil {
			return nil, nil, err
		}
		v := iter.Value()
		value := make([]byte, len(v))
		copy(value, v)
		// Converting from []byte -> string will copy the underlying data, so this is safe.
		keys = append(keys, string(iter.Key()))
		values = append(values, value)
	}
	return keys, values, iter.Close()
}

// GetWithPrefix gets all keys and values with the given prefix.
func (w *DataStore) GetWithPrefix(prefix string) ([]string, [][]byte, error) {
	return w.GetWithRange(prefix, string(keyUpperBound([]byte(prefix))))
}

// Delete deletes the value for the given key from the datastore.
func (w *DataStore) Delete(key string) error {
	return w.db.Delete([]byte(key), pebble.Sync)
}

// DeleteAll deletes all of the given keys and corresponding values in the datastore if they exist.
func (w *DataStore) DeleteAll(keys []string) error {
	batch := w.db.NewBatch()
	for _, key := range keys {
		batch.Delete([]byte(key), pebble.Sync)
	}
	return batch.Commit(pebble.Sync)
}

// DeleteWithPrefix deletes all keys and values with the given prefix.
func (w *DataStore) DeleteWithPrefix(prefix string) error {
	return w.db.DeleteRange([]byte(prefix), keyUpperBound([]byte(prefix)), pebble.Sync)
}

// Close stops the TTL watcher, and closes the underlying datastore.
// All other operations will fail after calling Close.
func (w *DataStore) Close() error {
	w.once.Do(func() {
		close(w.done)
	})

	if w.db == nil {
		return nil
	}

	db := w.db
	w.db = nil
	return db.Close()
}
