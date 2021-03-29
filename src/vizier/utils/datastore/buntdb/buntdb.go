package buntdb

import (
	"time"

	"github.com/tidwall/buntdb"
)

// DataStore wraps a buntdb datastore.
type DataStore struct {
	db *buntdb.DB
}

// New creates a new buntdb for use as a KVStore.
func New(db *buntdb.DB) *DataStore {
	wrap := &DataStore{
		db: db,
	}

	return wrap
}

// Get gets the value for the given key from the datastore.
func (w *DataStore) Get(key string) ([]byte, error) {
	var val []byte
	err := w.db.View(func(tx *buntdb.Tx) error {
		v, err := tx.Get(key)
		if err == buntdb.ErrNotFound {
			return nil
		}
		if err != nil {
			return err
		}
		val = []byte(v)
		return nil
	})
	return val, err
}

// GetWithRange gets all keys and values within the given range.
// Treats this as [from, to) i.e. includes the key from, but excludes the key to.
func (w *DataStore) GetWithRange(from string, to string) ([]string, [][]byte, error) {
	var keys []string
	var vals [][]byte

	err := w.db.View(func(tx *buntdb.Tx) error {
		return tx.AscendRange("", from, to, func(k, v string) bool {
			keys = append(keys, k)
			vals = append(vals, []byte(v))
			return true
		})
	})

	if err != nil {
		return nil, nil, err
	}
	return keys, vals, nil
}

// GetWithPrefix gets all keys and values with the given prefix.
func (w *DataStore) GetWithPrefix(prefix string) ([]string, [][]byte, error) {
	var keys []string
	var vals [][]byte

	err := w.db.View(func(tx *buntdb.Tx) error {
		return tx.AscendKeys(prefix+"*", func(k, v string) bool {
			keys = append(keys, k)
			vals = append(vals, []byte(v))
			return true
		})
	})

	if err != nil {
		return nil, nil, err
	}
	return keys, vals, nil
}

// Set puts the given key and value in the datastore.
func (w *DataStore) Set(key string, value string) error {
	return w.db.Update(func(tx *buntdb.Tx) error {
		_, _, err := tx.Set(key, value, nil)
		return err
	})
}

// SetWithTTL puts the given key and value into the datastore with a TTL.
// Once the TTL expires the datastore is expected to delete the given key and value.
func (w *DataStore) SetWithTTL(key string, value string, ttl time.Duration) error {
	return w.db.Update(func(tx *buntdb.Tx) error {
		_, _, err := tx.Set(key, value, &buntdb.SetOptions{
			Expires: true,
			TTL:     ttl,
		})
		return err
	})
}

// Delete deletes the value for the given key from the datastore.
func (w *DataStore) Delete(key string) error {
	return w.db.Update(func(tx *buntdb.Tx) error {
		_, err := tx.Delete(key)
		return err
	})
}

// DeleteAll deletes all of the given keys and corresponding values in the datastore if they exist.
func (w *DataStore) DeleteAll(keys []string) error {
	return w.db.Update(func(tx *buntdb.Tx) error {
		for i := 0; i < len(keys); i++ {
			_, err := tx.Delete(keys[i])
			if err != nil {
				return err
			}
		}
		return nil
	})
}

// DeleteWithPrefix deletes all keys and values with the given prefix.
func (w *DataStore) DeleteWithPrefix(prefix string) error {
	return w.db.Update(func(tx *buntdb.Tx) error {
		var keys []string
		err := tx.AscendKeys(prefix+"*", func(k, _ string) bool {
			keys = append(keys, k)
			return true
		})
		if err != nil {
			return err
		}
		for i := 0; i < len(keys); i++ {
			_, err := tx.Delete(keys[i])
			if err != nil {
				return err
			}
		}
		return nil
	})
}

// Close stops the TTL watcher, and closes the underlying datastore.
// All other operations will fail after calling Close.
func (w *DataStore) Close() error {
	err := w.db.Close()
	if err == buntdb.ErrDatabaseClosed {
		return nil
	}
	return err
}
