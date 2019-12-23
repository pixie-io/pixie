package kvstore_test

import (
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
	assert.Equal(t, "", val)

	val, err = c.Get("another_key")
	assert.Nil(t, err)
	assert.Equal(t, "abc", val)

	val, err = c.Get("non_ttl_key")
	assert.Nil(t, err)
	assert.Equal(t, "1234", val)
}

func TestCache_GetFromDatastore(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		Get("existing_key").
		Return("abcd", nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("new_key", "efgh", time.Second*10)

	val, err := c.Get("existing_key")
	assert.Nil(t, err)
	assert.Equal(t, "abcd", val)
}

func TestCache_Flush(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	expireTime := time.Unix(2, 0).Add(time.Second * 10)

	expectedFlush := map[string]kvstore.Entry{
		"abcd": kvstore.Entry{
			Value:     "efgh",
			ExpiresAt: expireTime,
		},
		"aKey": kvstore.Entry{
			Value:     "xyz",
			ExpiresAt: expireTime,
		},
		"non_ttl_key": kvstore.Entry{
			Value:     "1234",
			ExpiresAt: time.Time{},
		},
	}
	mockDs.
		EXPECT().
		SetAll(&expectedFlush).
		Return(nil)
	mockDs.
		EXPECT().
		Get("abcd").
		Return("efgh", nil)
	mockDs.
		EXPECT().
		Get("aKey").
		Return("xyz", nil)
	mockDs.
		EXPECT().
		Get("expiredKey").
		Return("", nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	c.SetWithTTL("abcd", "efgh", time.Second*10)
	c.SetWithTTL("aKey", "xyz", time.Second*10)
	c.SetWithTTL("expiredKey", "hello", time.Second*1)
	c.Set("non_ttl_key", "1234")
	clock.Advance(time.Second * 5)

	c.FlushToDatastore()

	val, err := c.Get("abcd")
	assert.Nil(t, err)
	assert.Equal(t, "efgh", val)

	val, err = c.Get("aKey")
	assert.Nil(t, err)
	assert.Equal(t, "xyz", val)

	val, err = c.Get("expiredKey")
	assert.Nil(t, err)
	assert.Equal(t, "", val)
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
		outputValues []string
		cacheKeys    []string
		cacheValues  []string
		cacheTTL     []time.Duration
		dsKeys       []string
		dsValues     []string
		hasError     bool
	}{
		{
			name:         "valid",
			prefix:       "a",
			outputKeys:   []string{"aa", "ab", "ac", "af", "ag", "ah"},
			outputValues: []string{"some", "value", "2", "3", "here", "4"},
			cacheKeys:    []string{"aa", "no", "ab", "ae", "ag", "non-matching"},
			cacheValues:  []string{"some", "test", "value", "goes", "here", "abc"},
			cacheTTL:     []time.Duration{time.Second * 0, time.Second * 0, time.Second * 10, time.Second * 1, time.Second * 0, time.Second * 10},
			dsKeys:       []string{"ab", "ac", "af", "ah"},
			dsValues:     []string{"1", "2", "3", "4"},
		},
		{
			name:         "datastore empty",
			prefix:       "a",
			outputKeys:   []string{"aa", "ab", "ag"},
			outputValues: []string{"some", "hi", "value"},
			cacheKeys:    []string{"aa", "ag", "ae", "ab"},
			cacheValues:  []string{"some", "value", "here", "hi"},
			cacheTTL:     []time.Duration{time.Second * 0, time.Second * 10, time.Second * 1, time.Second * 10},
			dsKeys:       []string{},
			dsValues:     []string{},
		},
		{
			name:         "cache empty",
			prefix:       "a",
			outputKeys:   []string{"ab", "ac", "af", "ah"},
			outputValues: []string{"1", "2", "3", "4"},
			cacheKeys:    []string{},
			cacheValues:  []string{},
			cacheTTL:     []time.Duration{},
			dsKeys:       []string{"ab", "ac", "af", "ah"},
			dsValues:     []string{"1", "2", "3", "4"},
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
