package kvstore_test

import (
	"bytes"
	"testing"
	"time"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore/mock"
)

func TestCache_GetFromCache(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("new_key", "efgh", time.Second*1)
	c.SetWithTTL("another_key", "abc", time.Second*10)
	c.Set("non_ttl_key", "1234")
	clock.Advance(time.Second * 5)

	val, err := c.Get("new_key")
	assert.Nil(t, err)
	assert.Nil(t, val)

	val, err = c.Get("another_key")
	assert.Nil(t, err)
	assert.Equal(t, "abc", string(val))

	val, err = c.Get("non_ttl_key")
	assert.Nil(t, err)
	assert.Equal(t, "1234", string(val))
}

func TestCache_GetFromDatastore(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		Get("existing_key").
		Return([]byte("abcd"), nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("new_key", "efgh", time.Second*10)

	val, err := c.Get("existing_key")
	assert.Nil(t, err)
	assert.Equal(t, "abcd", string(val))
}

func TestCache_Flush(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	expectedFlush := []kvstore.TTLKeyValue{
		kvstore.TTLKeyValue{
			Key:    "abcd",
			Value:  []byte("efgh"),
			Expire: true,
			TTL:    int64(5),
		},
		kvstore.TTLKeyValue{
			Key:    "aKey",
			Value:  []byte("xyz"),
			Expire: true,
			TTL:    int64(5),
		},
		kvstore.TTLKeyValue{
			Key:    "expiredKey",
			Value:  []byte("hello"),
			Expire: true,
			TTL:    int64(-4),
		},
		kvstore.TTLKeyValue{
			Key:    "non_ttl_key",
			Value:  []byte("1234"),
			Expire: false,
			TTL:    int64(0),
		},
		kvstore.TTLKeyValue{
			Key:    "deleted_key",
			Value:  []byte(nil),
			Expire: true,
			TTL:    int64(-5),
		},
	}

	mockDs.
		EXPECT().
		SetAll(gomock.Any()).
		DoAndReturn(func(kvs []kvstore.TTLKeyValue) error {
			assert.Equal(t, len(expectedFlush), len(kvs))
			for _, kv := range expectedFlush {
				seen := false
				for _, actualKv := range kvs {
					if actualKv.Key == kv.Key && actualKv.Expire == kv.Expire && actualKv.TTL == kv.TTL && bytes.Compare(actualKv.Value, kv.Value) == 0 {
						seen = true
					}
				}
				assert.Equal(t, true, seen)
			}

			return nil
		})

	mockDs.
		EXPECT().
		Get("abcd").
		Return([]byte("efgh"), nil)
	mockDs.
		EXPECT().
		Get("aKey").
		Return([]byte("xyz"), nil)
	mockDs.
		EXPECT().
		Get("expiredKey").
		Return(nil, nil)

	mockDs.
		EXPECT().
		Get("deleted_key").
		Return(nil, nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("abcd", "efgh", time.Second*10)
	c.SetWithTTL("aKey", "xyz", time.Second*10)
	c.SetWithTTL("expiredKey", "hello", time.Second*1)
	c.Set("non_ttl_key", "1234")
	c.DeleteAll([]string{"deleted_key"})
	clock.Advance(time.Second * 5)

	c.FlushToDatastore()

	val, err := c.Get("abcd")
	assert.Nil(t, err)
	assert.Equal(t, []byte("efgh"), val)

	val, err = c.Get("aKey")
	assert.Nil(t, err)
	assert.Equal(t, []byte("xyz"), val)

	val, err = c.Get("expiredKey")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("deleted_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)
}

func TestCache_GetPrefix(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	clock := testingutils.NewTestClock(time.Unix(2, 0))

	tests := []struct {
		name         string
		prefix       string
		outputKeys   []string
		outputValues [][]byte
		cacheKeys    []string
		cacheValues  []string
		cacheTTL     []time.Duration
		dsKeys       []string
		dsValues     [][]byte
		hasError     bool
	}{
		{
			name:         "valid",
			prefix:       "a",
			outputKeys:   []string{"aa", "ab", "ac", "af", "ag", "ah"},
			outputValues: [][]byte{[]byte("some"), []byte("value"), []byte("2"), []byte("3"), []byte("here"), []byte("4")},
			cacheKeys:    []string{"aa", "no", "ab", "ae", "ag", "non-matching"},
			cacheValues:  []string{"some", "test", "value", "goes", "here", "abc"},
			cacheTTL:     []time.Duration{time.Second * 0, time.Second * 0, time.Second * 10, time.Second * 1, time.Second * 0, time.Second * 10},
			dsKeys:       []string{"ab", "ac", "af", "ah"},
			dsValues:     [][]byte{[]byte("1"), []byte("2"), []byte("3"), []byte("4")},
		},
		{
			name:         "datastore empty",
			prefix:       "a",
			outputKeys:   []string{"aa", "ab", "ag"},
			outputValues: [][]byte{[]byte("some"), []byte("hi"), []byte("value")},
			cacheKeys:    []string{"aa", "ag", "ae", "ab"},
			cacheValues:  []string{"some", "value", "here", "hi"},
			cacheTTL:     []time.Duration{time.Second * 0, time.Second * 10, time.Second * 1, time.Second * 10},
			dsKeys:       []string{},
			dsValues:     [][]byte{},
		},
		{
			name:         "cache empty",
			prefix:       "a",
			outputKeys:   []string{"ab", "ac", "af", "ah"},
			outputValues: [][]byte{[]byte("1"), []byte("2"), []byte("3"), []byte("4")},
			cacheKeys:    []string{},
			cacheValues:  []string{},
			cacheTTL:     []time.Duration{},
			dsKeys:       []string{"ab", "ac", "af", "ah"},
			dsValues:     [][]byte{[]byte("1"), []byte("2"), []byte("3"), []byte("4")},
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			c := kvstore.NewCacheWithClock(mockDs, clock)

			mockDs.
				EXPECT().
				GetWithPrefix(tc.prefix).
				Return(tc.dsKeys, tc.dsValues, nil)

			for i, key := range tc.cacheKeys {
				if tc.cacheTTL[i] > time.Second*0 {
					c.SetWithTTL(key, tc.cacheValues[i], tc.cacheTTL[i])
				} else {
					c.Set(key, tc.cacheValues[i])
				}
			}
			clock.Advance(time.Second * 5)

			outKeys, outVals, err := c.GetWithPrefix(tc.prefix)
			assert.Nil(t, err)
			assert.Equal(t, tc.outputKeys, outKeys)
			assert.Equal(t, tc.outputValues, outVals)
		})
	}
}

func TestCache_GetRange(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	clock := testingutils.NewTestClock(time.Unix(2, 0))

	tests := []struct {
		name         string
		from         string
		to           string
		outputKeys   []string
		outputValues [][]byte
		cacheKeys    []string
		cacheValues  []string
		cacheTTL     []time.Duration
		dsKeys       []string
		dsValues     [][]byte
		hasError     bool
	}{
		{
			name:         "valid",
			from:         "2",
			to:           "6",
			outputKeys:   []string{"2", "3", "5"},
			outputValues: [][]byte{[]byte("2Cache"), []byte("3DS"), []byte("5Cache")},
			cacheKeys:    []string{"1", "2", "4", "5"},
			cacheValues:  []string{"1Cache", "2Cache", "4Cache", "5Cache"},
			cacheTTL:     []time.Duration{time.Second * 0, time.Second * 0, time.Second * 1, time.Second * 10},
			dsKeys:       []string{"3", "5"},
			dsValues:     [][]byte{[]byte("3DS"), []byte("5DS")},
		},
		{
			name:         "datastore empty",
			from:         "2",
			to:           "6",
			outputKeys:   []string{"2", "5"},
			outputValues: [][]byte{[]byte("2Cache"), []byte("5Cache")},
			cacheKeys:    []string{"1", "2", "4", "5"},
			cacheValues:  []string{"1Cache", "2Cache", "4Cache", "5Cache"},
			cacheTTL:     []time.Duration{time.Second * 0, time.Second * 0, time.Second * 1, time.Second * 10},
			dsKeys:       []string{},
			dsValues:     [][]byte{},
		},
		{
			name:         "cache empty",
			from:         "2",
			to:           "6",
			outputKeys:   []string{"2", "5"},
			outputValues: [][]byte{[]byte("2Cache"), []byte("5Cache")},
			cacheKeys:    []string{"1", "2", "4", "5"},
			cacheValues:  []string{"1Cache", "2Cache", "4Cache", "5Cache"},
			cacheTTL:     []time.Duration{time.Second * 0, time.Second * 0, time.Second * 1, time.Second * 10},
			dsKeys:       []string{},
			dsValues:     [][]byte{},
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			c := kvstore.NewCacheWithClock(mockDs, clock)

			mockDs.
				EXPECT().
				GetWithRange(tc.from, tc.to).
				Return(tc.dsKeys, tc.dsValues, nil)

			for i, key := range tc.cacheKeys {
				if tc.cacheTTL[i] > time.Second*0 {
					c.SetWithTTL(key, tc.cacheValues[i], tc.cacheTTL[i])
				} else {
					c.Set(key, tc.cacheValues[i])
				}
			}
			clock.Advance(time.Second * 5)

			outKeys, outVals, err := c.GetWithRange(tc.from, tc.to)
			assert.Nil(t, err)
			assert.Equal(t, tc.outputKeys, outKeys)
			assert.Equal(t, tc.outputValues, outVals)
		})
	}
}

func TestCache_GetAll(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("new_key", "efgh", time.Second*1)
	c.SetWithTTL("another_key", "abc", time.Second*10)
	c.Set("non_ttl_key", "1234")
	clock.Advance(time.Second * 5)

	val, err := c.GetAll([]string{"another_key", "non_ttl_key"})
	assert.Nil(t, err)
	assert.Equal(t, [][]byte{[]byte("abc"), []byte("1234")}, val)

	mockDs.
		EXPECT().
		GetAll([]string{"non_ttl_key", "non_existent_key", "key_in_db"}).
		Return([][]byte{[]byte("old_value"), nil, []byte("value_in_db")}, nil)

	val, err = c.GetAll([]string{"non_ttl_key", "non_existent_key", "key_in_db"})
	assert.Nil(t, err)
	assert.Equal(t, [][]byte{[]byte("1234"), nil, []byte("value_in_db")}, val)
}

func TestCache_DeleteAll(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("new_key", "efgh", time.Second*1)
	c.SetWithTTL("another_key", "abc", time.Second*10)
	c.SetWithTTL("a_key", "xyz", time.Second*10)
	c.Set("non_ttl_key", "1234")
	clock.Advance(time.Second * 5)

	c.DeleteAll([]string{"new_key", "another_key", "non_ttl_key"})
	clock.Advance(time.Second * 1)

	val, err := c.Get("new_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("another_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("non_ttl_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("a_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte("xyz"), val)
}

func TestCache_UncachedDelete(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mockDs.
		EXPECT().
		Delete("key").
		Return(nil)

	err := c.UncachedDelete("key")
	assert.Nil(t, err)
}

func TestCache_UncachedDeleteAll(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mockDs.
		EXPECT().
		SetAll([]kvstore.TTLKeyValue{
			kvstore.TTLKeyValue{
				Expire: true,
				TTL:    0,
				Value:  []byte{},
				Key:    "key1",
			},
			kvstore.TTLKeyValue{
				Expire: true,
				TTL:    0,
				Value:  []byte{},
				Key:    "key2",
			},
		}).
		Return(nil)

	err := c.UncachedDeleteAll([]string{"key1", "key2"})
	assert.Nil(t, err)
}

func TestCache_UncachedSet(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mockDs.
		EXPECT().
		SetAll([]kvstore.TTLKeyValue{kvstore.TTLKeyValue{"key", []byte("value"), false, 0}}).
		Return(nil)

	err := c.UncachedSet("key", "value")
	assert.Nil(t, err)
}

func TestCache_UncachedSetWithTTL(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mockDs.
		EXPECT().
		SetAll([]kvstore.TTLKeyValue{kvstore.TTLKeyValue{"key", []byte("value"), true, 1}}).
		Return(nil)

	err := c.UncachedSetWithTTL("key", "value", 1*time.Second)
	assert.Nil(t, err)
}

func TestCache_DeleteWithPrefix(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("anew_key", "efgh", time.Second*1)
	c.SetWithTTL("another_key", "abc", time.Second*10)
	c.SetWithTTL("a_key", "xyz", time.Second*10)
	c.Set("non_ttl_key", "1234")

	mockDs.
		EXPECT().
		DeleteWithPrefix("a").
		Return(nil)
	mockDs.
		EXPECT().
		Get("anew_key").
		Return(nil, nil)
	mockDs.
		EXPECT().
		Get("another_key").
		Return(nil, nil)
	mockDs.
		EXPECT().
		Get("a_key").
		Return(nil, nil)

	err := c.DeleteWithPrefix("a")
	assert.Nil(t, err)

	val, err := c.Get("anew_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("another_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("a_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("non_ttl_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte("1234"), val)
}

func TestCache_Clear(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("anew_key", "efgh", time.Second*1)
	c.SetWithTTL("another_key", "abc", time.Second*10)
	c.SetWithTTL("a_key", "xyz", time.Second*10)
	c.Set("non_ttl_key", "1234")

	mockDs.
		EXPECT().
		Get("anew_key").
		Return(nil, nil)
	mockDs.
		EXPECT().
		Get("another_key").
		Return(nil, nil)
	mockDs.
		EXPECT().
		Get("a_key").
		Return(nil, nil)
	mockDs.
		EXPECT().
		Get("non_ttl_key").
		Return(nil, nil)

	c.Clear()

	val, err := c.Get("anew_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("another_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("a_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)

	val, err = c.Get("non_ttl_key")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)
}
