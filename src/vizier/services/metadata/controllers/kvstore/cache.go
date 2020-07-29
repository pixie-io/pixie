package kvstore

import (
	"errors"
	"sort"
	"strings"
	"sync"
	"time"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/utils"
)

// CacheFlushMaxRetries is the maximum number of time the cache should attempt flushing its data
// before terminating.
const CacheFlushMaxRetries int64 = 5

// EventType is an action that can occur to a key.
type EventType int

const (
	// EventTypeDelete is when a key is deleted.
	EventTypeDelete EventType = iota
	// EventTypePut is when a key is put into the store.
	EventTypePut
)

// KeyEvent is an event associated with a key in the key value store.
type KeyEvent struct {
	EventType EventType
	Key       string
}

// KeyValueStore is a key value store which supports getting and setting multiple values.
type KeyValueStore interface {
	Get(string) ([]byte, error)
	SetAll([]TTLKeyValue) error
	GetWithPrefix(string) ([]string, [][]byte, error)
	GetWithRange(string, string) ([]string, [][]byte, error)
	GetAll([]string) ([][]byte, error)
	DeleteWithPrefix(string) error
	WatchKeyEvents(string) (chan KeyEvent, chan bool)
	Delete(string) error
}

type entry struct {
	Value     []byte
	ExpiresAt time.Time
}

// TTLKeyValue is a key value entry with a TTL.
type TTLKeyValue struct {
	Key    string
	Value  []byte
	Expire bool // Whether or not this key-value should expire.
	TTL    int64
}

// Cache is a cache for a key value store. It periodically flushes data to the key value store.
type Cache struct {
	cacheMap   map[string]entry
	datastore  KeyValueStore
	cacheMu    sync.RWMutex
	dsMu       sync.RWMutex
	clock      utils.Clock
	numRetries int64
}

// NewCache creates a new cache.
func NewCache(datastore KeyValueStore) *Cache {
	clock := utils.SystemClock{}
	return NewCacheWithClock(datastore, clock)
}

// NewCacheWithClock creates a new cache with a clock. Used for testing purposes.
func NewCacheWithClock(datastore KeyValueStore, clock utils.Clock) *Cache {
	c := &Cache{
		cacheMap:  make(map[string]entry),
		datastore: datastore,
		clock:     clock,
	}

	return c
}

// FlushToDatastore flushes the entries in cache to the datastore.
func (c *Cache) FlushToDatastore() {
	c.cacheMu.Lock()
	defer c.cacheMu.Unlock()
	c.dsMu.Lock()
	defer c.dsMu.Unlock()

	entries := make([]TTLKeyValue, len(c.cacheMap))
	// Process values in cache into TTLKeyValues.
	now := c.clock.Now()
	i := 0
	for k, v := range c.cacheMap {
		ttl := int64(v.ExpiresAt.Sub(now).Seconds())
		if v.ExpiresAt.IsZero() {
			ttl = 0
		}
		entries[i] = TTLKeyValue{k, v.Value, !v.ExpiresAt.IsZero(), ttl}
		i++
	}

	err := c.datastore.SetAll(entries)

	if err != nil {
		c.numRetries++
		if c.numRetries >= CacheFlushMaxRetries {
			log.Fatal("Cache flush: exceeded max number of retries.")
		}
		log.WithError(err).Error("Could not flush cache to datastore.")
	} else {
		// Clear out cache.
		c.cacheMap = make(map[string]entry)
		c.numRetries = 0
	}
}

// Get gets the given key from the cache or the backing datastore.
func (c *Cache) Get(key string) ([]byte, error) {
	c.cacheMu.RLock()
	c.dsMu.RLock()

	defer c.dsMu.RUnlock()

	cacheVal, err := func() ([]byte, error) {
		defer c.cacheMu.RUnlock()

		if val, ok := c.cacheMap[key]; ok {
			if !val.ExpiresAt.IsZero() && val.ExpiresAt.Before(c.clock.Now()) {
				return nil, nil
			}
			return val.Value, nil
		}
		return nil, errors.New("Did not find value in cache")
	}()

	if err == nil {
		return cacheVal, err
	}

	return c.datastore.Get(key)
}

// SetWithTTL puts the given key and value in the cache, which is later flushed to the backing datastore.
// A TTL of 0 means that the key is deleted.
func (c *Cache) SetWithTTL(key string, value string, ttl time.Duration) {
	c.cacheMu.Lock()
	defer c.cacheMu.Unlock()

	c.cacheMap[key] = entry{
		Value:     []byte(value),
		ExpiresAt: c.clock.Now().Add(ttl),
	}
}

// UncachedSetWithTTL sets the key directly in the underlying datastore with the given TTL.
func (c *Cache) UncachedSetWithTTL(key string, value string, ttl time.Duration) error {
	c.dsMu.Lock()
	defer c.dsMu.Unlock()

	entries := []TTLKeyValue{
		TTLKeyValue{key, []byte(value), true, int64(ttl.Seconds())},
	}
	return c.datastore.SetAll(entries)
}

// UncachedSet sets the key directly in the underlying datastore with no TTL.
func (c *Cache) UncachedSet(key string, value string) error {
	c.dsMu.Lock()
	defer c.dsMu.Unlock()

	entries := []TTLKeyValue{
		TTLKeyValue{key, []byte(value), false, 0},
	}
	return c.datastore.SetAll(entries)
}

// UncachedDelete deletes the key directly in the underlying datastore.
func (c *Cache) UncachedDelete(key string) error {
	c.dsMu.Lock()
	defer c.dsMu.Unlock()

	return c.datastore.Delete(key)
}

// UncachedDeleteAll does a transactionalized delete of all of the given keys directly in the underlying datastore.
func (c *Cache) UncachedDeleteAll(keys []string) error {
	c.dsMu.Lock()
	defer c.dsMu.Unlock()

	ttlKeys := make([]TTLKeyValue, len(keys))
	for i, key := range keys {
		ttlKeys[i] = TTLKeyValue{
			Expire: true,
			TTL:    0,
			Value:  []byte{},
			Key:    key,
		}
	}

	return c.datastore.SetAll(ttlKeys)
}

// Set puts the given key and value in the cache, which is later flushed to the backing datastore.
func (c *Cache) Set(key string, value string) {
	c.cacheMu.Lock()
	defer c.cacheMu.Unlock()

	c.cacheMap[key] = entry{
		Value:     []byte(value),
		ExpiresAt: time.Time{}, // The zero value of time should represent no TTL.
	}
}

// DeleteAll deletes all of the keys in the cache and datastore if they exist.
func (c *Cache) DeleteAll(keys []string) {
	for _, key := range keys {
		c.SetWithTTL(key, "", 0)
	}
}

// GetAll get the values for all of the given keys and returns them in the given order. If no value exists
// for the given key, it returns an empty string in that index.
func (c *Cache) GetAll(keys []string) ([][]byte, error) {
	vals := make([][]byte, len(keys))
	cacheHits := 0

	c.cacheMu.RLock()
	c.dsMu.RLock()

	defer c.dsMu.RUnlock()

	func() {
		defer c.cacheMu.RUnlock()
		for i, k := range keys {
			if val, ok := c.cacheMap[k]; ok {
				if val.ExpiresAt.IsZero() || !val.ExpiresAt.Before(c.clock.Now()) {
					vals[i] = val.Value
					cacheHits++
				}
			}
		}
	}()

	if cacheHits == len(keys) {
		return vals, nil
	}

	// Make a request to backing datastore and fill in missing values.
	dVals, err := c.datastore.GetAll(keys)
	if err != nil {
		return nil, err
	}
	for i, val := range dVals {
		if vals[i] == nil && val != nil {
			vals[i] = val
		}
	}

	return vals, nil
}

// GetWithRange gets all keys and values within the given range.
func (c *Cache) GetWithRange(from string, to string) (keys []string, values [][]byte, err error) {
	keys = make([]string, 0)
	values = make([][]byte, 0)
	cacheCopy := make(map[string]entry)
	cacheKeys := make([]string, 0)

	c.cacheMu.RLock()
	c.dsMu.RLock()

	defer c.dsMu.RUnlock()

	func() {
		defer c.cacheMu.RUnlock()
		for key := range c.cacheMap {
			if key >= from && key < to {
				cacheKeys = append(cacheKeys, key)
				cacheCopy[key] = c.cacheMap[key]
			}
		}
	}()

	sort.Strings(cacheKeys)

	// Get the keys/values with prefix from the backing datastore. This assumes
	// the response is already sorted by key.
	dsKeys, dsVals, err := c.datastore.GetWithRange(from, to)
	if err != nil {
		return keys, values, err
	}

	return c.mergeCacheAndStore(cacheCopy, cacheKeys, dsKeys, dsVals)
}

func (c *Cache) mergeCacheAndStore(cacheCopy map[string]entry, cacheKeys []string, dsKeys []string, dsVals [][]byte) (keys []string, values [][]byte, err error) {
	keys = make([]string, 0)
	values = make([][]byte, 0)

	now := c.clock.Now()

	// We need to merge the results from the cache and datastore. This performs
	// the merging by iterating through each key from the cache/datastore in sorted order.
	cacheIdx := 0
	dsIdx := 0
	for {
		if dsIdx == len(dsKeys) && cacheIdx == len(cacheKeys) {
			break
		}

		if dsIdx == len(dsKeys) || (cacheIdx != len(cacheKeys) && cacheKeys[cacheIdx] < dsKeys[dsIdx]) {
			cacheEntry := cacheCopy[cacheKeys[cacheIdx]]
			if cacheEntry.ExpiresAt.IsZero() || !cacheEntry.ExpiresAt.Before(now) {
				keys = append(keys, cacheKeys[cacheIdx])
				values = append(values, cacheEntry.Value)
			}
			cacheIdx++
			continue
		}
		if cacheIdx == len(cacheKeys) || (dsKeys[dsIdx] < cacheKeys[cacheIdx]) {
			keys = append(keys, dsKeys[dsIdx])
			values = append(values, dsVals[dsIdx])
			dsIdx++
			continue
		}
		if dsKeys[dsIdx] == cacheKeys[cacheIdx] {
			// If the key already exists in the backing datastore, the
			// version in the cache is more recent.
			cacheEntry := cacheCopy[cacheKeys[cacheIdx]]
			if cacheEntry.ExpiresAt.IsZero() || !cacheEntry.ExpiresAt.Before(now) {
				keys = append(keys, cacheKeys[cacheIdx])
				values = append(values, cacheEntry.Value)
			}
			dsIdx++
			cacheIdx++
			continue
		}
	}

	return keys, values, nil
}

// GetWithPrefix gets all keys and values with the given prefix.
func (c *Cache) GetWithPrefix(prefix string) (keys []string, values [][]byte, err error) {
	keys = make([]string, 0)
	values = make([][]byte, 0)

	// Find all keys in cache beginning with prefix.
	cacheKeys := make([]string, 0)
	cacheCopy := make(map[string]entry)

	c.cacheMu.RLock()
	c.dsMu.RLock()

	defer c.dsMu.RUnlock()

	func() {
		defer c.cacheMu.RUnlock()
		for key := range c.cacheMap {
			if strings.HasPrefix(key, prefix) {
				cacheKeys = append(cacheKeys, key)
				cacheCopy[key] = c.cacheMap[key]
			}
		}
	}()

	sort.Strings(cacheKeys)

	// Get the keys/values with prefix from the backing datastore. This assumes
	// the response is already sorted by key.
	dsKeys, dsVals, err := c.datastore.GetWithPrefix(prefix)
	if err != nil {
		return keys, values, err
	}

	return c.mergeCacheAndStore(cacheCopy, cacheKeys, dsKeys, dsVals)
}

// DeleteWithPrefix deletes all keys and values with the given prefix.
func (c *Cache) DeleteWithPrefix(prefix string) error {
	c.cacheMu.Lock()
	defer c.cacheMu.Unlock()
	c.dsMu.Lock()
	defer c.dsMu.Unlock()

	for key := range c.cacheMap {
		if strings.HasPrefix(key, prefix) {
			delete(c.cacheMap, key)
		}
	}

	return c.datastore.DeleteWithPrefix(prefix)
}

// Clear deletes all keys and values from the cache.
func (c *Cache) Clear() {
	c.cacheMu.Lock()
	defer c.cacheMu.Unlock()

	c.cacheMap = make(map[string]entry)
}

// WatchKeyEvents watches key events in the underlying datastore.
func (c *Cache) WatchKeyEvents(prefix string) (chan KeyEvent, chan bool) {
	return c.datastore.WatchKeyEvents(prefix)
}
