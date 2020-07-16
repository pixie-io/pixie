package kvstore

import (
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

// KeyValueStore is a key value store which supports getting and setting multiple values.
type KeyValueStore interface {
	Get(string) ([]byte, error)
	SetAll([]TTLKeyValue) error
	GetWithPrefix(string) ([]string, [][]byte, error)
	GetWithRange(string, string) ([]string, [][]byte, error)
	GetAll([]string) ([][]byte, error)
	DeleteWithPrefix(string) error
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
	mu         sync.RWMutex
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
	c.mu.Lock()
	defer c.mu.Unlock()

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
	c.mu.RLock()
	defer c.mu.RUnlock()

	if val, ok := c.cacheMap[key]; ok {
		if !val.ExpiresAt.IsZero() && val.ExpiresAt.Before(c.clock.Now()) {
			return nil, nil
		}
		return val.Value, nil
	}

	return c.datastore.Get(key)
}

// SetWithTTL puts the given key and value in the cache, which is later flushed to the backing datastore.
// A TTL of 0 means that the key is deleted.
func (c *Cache) SetWithTTL(key string, value string, ttl time.Duration) {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.cacheMap[key] = entry{
		Value:     []byte(value),
		ExpiresAt: c.clock.Now().Add(ttl),
	}
}

// Set puts the given key and value in the cache, which is later flushed to the backing datastore.
func (c *Cache) Set(key string, value string) {
	c.mu.Lock()
	defer c.mu.Unlock()

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

	c.mu.RLock()
	defer c.mu.RUnlock()
	cacheHits := 0
	for i, k := range keys {
		if val, ok := c.cacheMap[k]; ok {
			if val.ExpiresAt.IsZero() || !val.ExpiresAt.Before(c.clock.Now()) {
				vals[i] = val.Value
				cacheHits++
			}
		}
	}

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
	c.mu.RLock()
	defer c.mu.RUnlock()

	keys = make([]string, 0)
	values = make([][]byte, 0)

	// Find all keys in cache within the given range.
	cacheKeys := make([]string, 0)
	for key := range c.cacheMap {
		if key >= from && key < to {
			cacheKeys = append(cacheKeys, key)
		}
	}
	sort.Strings(cacheKeys)

	// Get the keys/values with prefix from the backing datastore. This assumes
	// the response is already sorted by key.
	dsKeys, dsVals, err := c.datastore.GetWithRange(from, to)
	if err != nil {
		return keys, values, err
	}

	return c.mergeCacheAndStore(cacheKeys, dsKeys, dsVals)
}

func (c *Cache) mergeCacheAndStore(cacheKeys []string, dsKeys []string, dsVals [][]byte) (keys []string, values [][]byte, err error) {
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
			cacheEntry := c.cacheMap[cacheKeys[cacheIdx]]
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
			cacheEntry := c.cacheMap[cacheKeys[cacheIdx]]
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
	c.mu.RLock()
	defer c.mu.RUnlock()

	keys = make([]string, 0)
	values = make([][]byte, 0)

	// Find all keys in cache beginning with prefix.
	cacheKeys := make([]string, 0)
	for key := range c.cacheMap {
		if strings.HasPrefix(key, prefix) {
			cacheKeys = append(cacheKeys, key)
		}
	}
	sort.Strings(cacheKeys)

	// Get the keys/values with prefix from the backing datastore. This assumes
	// the response is already sorted by key.
	dsKeys, dsVals, err := c.datastore.GetWithPrefix(prefix)
	if err != nil {
		return keys, values, err
	}

	return c.mergeCacheAndStore(cacheKeys, dsKeys, dsVals)
}

// DeleteWithPrefix deletes all keys and values with the given prefix.
func (c *Cache) DeleteWithPrefix(prefix string) error {
	c.mu.Lock()
	defer c.mu.Unlock()

	for key := range c.cacheMap {
		if strings.HasPrefix(key, prefix) {
			delete(c.cacheMap, key)
		}
	}

	return c.datastore.DeleteWithPrefix(prefix)
}

// Clear deletes all keys and values from the cache.
func (c *Cache) Clear() {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.cacheMap = make(map[string]entry)
}
