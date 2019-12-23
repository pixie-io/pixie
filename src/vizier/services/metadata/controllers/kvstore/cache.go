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
	Get(string) (string, error)
	SetAll(*map[string]Entry) error
	GetWithPrefix(string) ([]string, []string, error)
}

// Entry is an entry value + ttl inside a key value store.
type Entry struct {
	Value     string
	ExpiresAt time.Time
}

// Cache is a cache for a key value store. It periodically flushes data to the key value store.
type Cache struct {
	cacheMap   map[string]Entry
	datastore  KeyValueStore
	mu         sync.Mutex
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
		cacheMap:  make(map[string]Entry),
		datastore: datastore,
		clock:     clock,
	}

	return c
}

// FlushToDatastore flushes the entries in cache to the datastore.
func (c *Cache) FlushToDatastore() {
	c.mu.Lock()
	defer c.mu.Unlock()

	cachedData := c.cacheMap

	now := c.clock.Now()

	// Don't bothering flushing expired data to the backing datastore.
	for k, v := range cachedData {
		if !v.ExpiresAt.IsZero() && v.ExpiresAt.Before(now) {
			delete(cachedData, k)
		}
	}
	err := c.datastore.SetAll(&cachedData)

	if err != nil {
		c.numRetries++
		if c.numRetries >= CacheFlushMaxRetries {
			log.Fatal("Cache flush: exceeded max number of retries.")
		}
		log.WithError(err).Error("Could not flush cache to datastore.")
	} else {
		// Clear out cache.
		c.cacheMap = make(map[string]Entry)
		c.numRetries = 0
	}
}

// Get gets the given key from the cache or the backing datastore.
func (c *Cache) Get(key string) (string, error) {
	c.mu.Lock()
	defer c.mu.Unlock()

	if val, ok := c.cacheMap[key]; ok {
		if !val.ExpiresAt.IsZero() && val.ExpiresAt.Before(c.clock.Now()) {
			return "", nil
		}
		return val.Value, nil
	}

	return c.datastore.Get(key)
}

// SetWithTTL puts the given key and value in the cache, which is later flushed to the backing datastore.
func (c *Cache) SetWithTTL(key string, value string, ttl time.Duration) {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.cacheMap[key] = Entry{
		Value:     value,
		ExpiresAt: c.clock.Now().Add(ttl),
	}
}

// Set puts the given key and value in the cache, which is later flushed to the backing datastore.
func (c *Cache) Set(key string, value string) {
	c.mu.Lock()
	defer c.mu.Unlock()

	c.cacheMap[key] = Entry{
		Value:     value,
		ExpiresAt: time.Time{}, // The zero value of time should represent no TTL.
	}
}

// GetWithPrefix gets all keys and values with the given prefix.
func (c *Cache) GetWithPrefix(prefix string) (keys []string, values []string, err error) {
	keys = make([]string, 0)
	values = make([]string, 0)

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
