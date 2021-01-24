package etcd

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestEtcd(t *testing.T) {
	c, cleanup := testingutils.SetupEtcd()
	defer cleanup()

	db, err := New(c)
	if err != nil {
		t.Fatal("failed to initialize etcd")
	}

	t.Run("Set/Get", func(t *testing.T) {
		db.Set("key1", "val1")
		db.Set("key2", "val2")

		v, err := db.Get("key1")
		if assert.NoError(t, err) {
			assert.Equal(t, "val1", string(v))
		}

		v, err = db.Get("key2")
		if assert.NoError(t, err) {
			assert.Equal(t, "val2", string(v))
		}

		db.Set("key1", "val1.1")

		v, err = db.Get("key1")
		if assert.NoError(t, err) {
			assert.Equal(t, "val1.1", string(v))
		}

		v, err = db.Get("nonexistent")
		if assert.NoError(t, err) {
			assert.Nil(t, v)
		}
	})

	t.Run("SetWithTTL", func(t *testing.T) {
		now := time.Now()
		ttl := 10 * time.Second

		db.SetWithTTL("timed1", "limited1", ttl)

		timedOut := time.After(1 * time.Minute)
		ticker := time.NewTicker(1 * time.Second)
		defer ticker.Stop()

		for {
			select {
			case <-timedOut:
				t.Error("set with TTL timed out and key still exists")
				return
			case <-ticker.C:
				v, err := db.Get("timed1")
				if assert.NoError(t, err) {
					if time.Since(now) < ttl {
						assert.Equal(t, "limited1", string(v))
					} else if v == nil {
						// Key was deleted some time after TTL passed.
						return
					}
				}
			}
		}
	})

	t.Run("Get", func(t *testing.T) {
		db.Set("jam1", "nil")
		db.Set("key1", "val1")
		db.Set("key2", "val2")
		db.Set("key3", "val3")
		db.Set("key9", "val9")
		db.Set("lim1", "inf")

		t.Run("Range", func(t *testing.T) {
			keys, vals, err := db.GetWithRange("key1", "key1.1")
			if assert.NoError(t, err) {
				assert.Equal(t, []string{"key1"}, keys)
				assert.Equal(t, [][]byte{[]byte("val1")}, vals)
			}

			keys, vals, err = db.GetWithRange("key1", "key2")
			if assert.NoError(t, err) {
				assert.Equal(t, []string{"key1"}, keys)
				assert.Equal(t, [][]byte{[]byte("val1")}, vals)
			}

			keys, vals, err = db.GetWithRange("key1", "key4")
			if assert.NoError(t, err) {
				assert.Equal(t, []string{"key1", "key2", "key3"}, keys)
				assert.Equal(t, [][]byte{[]byte("val1"), []byte("val2"), []byte("val3")}, vals)
			}

			keys, vals, err = db.GetWithRange("nonexistent", "nonexistent2")
			if assert.NoError(t, err) {
				assert.Nil(t, keys)
				assert.Nil(t, vals)
			}
		})

		t.Run("Prefix", func(t *testing.T) {
			keys, vals, err := db.GetWithPrefix("key")
			if assert.NoError(t, err) {
				assert.Equal(t, []string{"key1", "key2", "key3", "key9"}, keys)
				assert.Equal(t, [][]byte{[]byte("val1"), []byte("val2"), []byte("val3"), []byte("val9")}, vals)
			}

			keys, vals, err = db.GetWithPrefix("nonexistent")
			if assert.NoError(t, err) {
				assert.Nil(t, keys)
				assert.Nil(t, vals)
			}
		})
	})

	t.Run("Delete", func(t *testing.T) {
		db.Set("jam1", "neg")
		db.Set("key1", "val1")
		db.Set("key2", "val2")
		db.Set("key3", "val3")
		db.Set("key9", "val9")
		db.Set("lim1", "inf")

		err := db.Delete("key2")
		if assert.NoError(t, err) {
			v, err := db.Get("key2")
			if assert.NoError(t, err) {
				assert.Nil(t, v)
			}

			v, err = db.Get("key1")
			if assert.NoError(t, err) {
				assert.Equal(t, "val1", string(v))
			}
		}
	})

	t.Run("DeleteAll", func(t *testing.T) {
		db.Set("jam1", "neg")
		db.Set("key1", "val1")
		db.Set("key2", "val2")
		db.Set("key3", "val3")
		db.Set("key9", "val9")
		db.Set("lim1", "inf")

		err := db.DeleteAll([]string{"key1", "key3"})
		if assert.NoError(t, err) {
			v, err := db.Get("key1")
			if assert.NoError(t, err) {
				assert.Nil(t, v)
			}

			v, err = db.Get("key2")
			if assert.NoError(t, err) {
				assert.Equal(t, "val2", string(v))
			}
		}
	})

	t.Run("DeletePrefix", func(t *testing.T) {
		db.Set("jam1", "neg")
		db.Set("key1", "val1")
		db.Set("key2", "val2")
		db.Set("key3", "val3")
		db.Set("key9", "val9")
		db.Set("lim1", "inf")

		err := db.DeleteWithPrefix("key")

		if assert.NoError(t, err) {
			v, err := db.Get("key1")
			if assert.NoError(t, err) {
				assert.Nil(t, v)
			}

			v, err = db.Get("key2")
			if assert.NoError(t, err) {
				assert.Nil(t, v)
			}

			v, err = db.Get("jam1")
			if assert.NoError(t, err) {
				assert.Equal(t, "neg", string(v))
			}
		}
	})

	err = db.Close()
	assert.NoError(t, err)

	// Calling close repeatedly should be fine
	err = db.Close()
	assert.NoError(t, err)
}
