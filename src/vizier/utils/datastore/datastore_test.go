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

package datastore

import (
	"fmt"
	"testing"
	"time"

	"github.com/cockroachdb/pebble"
	"github.com/cockroachdb/pebble/vfs"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/utils/datastore/etcd"
	"px.dev/pixie/src/vizier/utils/datastore/pebbledb"
)

func setupDatastore(t *testing.T, db Setter) {
	err := db.Set("jam1", "neg")
	require.NoError(t, err)
	err = db.Set("key1", "val1")
	require.NoError(t, err)
	err = db.Set("key2", "val2")
	require.NoError(t, err)
	err = db.Set("key3", "val3")
	require.NoError(t, err)
	err = db.Set("key9", "val9")
	require.NoError(t, err)
	err = db.Set("lim1", "inf")
	require.NoError(t, err)
}

func TestDatastore(t *testing.T) {
	memFS := vfs.NewMem()
	pbbl, err := pebble.Open("test", &pebble.Options{
		FS: memFS,
	})
	if err != nil {
		t.Fatal("failed to initialize a pebbledb")
	}

	et, cleanup, err := testingutils.SetupEtcd()
	if err != nil {
		t.Fatal("failed to initialize an embedded etcd")
	}
	defer cleanup()

	tests := []struct {
		db          MultiGetterSetterDeleterCloser
		name        string
		runTTLTests bool
	}{
		{pebbledb.New(pbbl, 2*time.Second), "PebbleDB", true},
		{etcd.New(et), "etcd", false},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			db := tc.db
			t.Run("Set/Get", func(t *testing.T) {
				setupDatastore(t, db)
				v, err := db.Get("key1")
				require.NoError(t, err)
				assert.Equal(t, "val1", string(v))

				v, err = db.Get("key2")
				require.NoError(t, err)
				assert.Equal(t, "val2", string(v))

				err = db.Set("key1", "val1.1")
				require.NoError(t, err)

				v, err = db.Get("key1")
				require.NoError(t, err)
				assert.Equal(t, "val1.1", string(v))

				v, err = db.Get("nonexistent")
				require.NoError(t, err)
				assert.Nil(t, v)
			})

			t.Run("Get", func(t *testing.T) {
				setupDatastore(t, db)
				t.Run("Range", func(t *testing.T) {
					keys, vals, err := db.GetWithRange("key1", "key1.1")
					require.NoError(t, err)
					assert.Equal(t, []string{"key1"}, keys)
					assert.Equal(t, [][]byte{[]byte("val1")}, vals)

					keys, vals, err = db.GetWithRange("key1", "key2")
					require.NoError(t, err)
					assert.Equal(t, []string{"key1"}, keys)
					assert.Equal(t, [][]byte{[]byte("val1")}, vals)

					keys, vals, err = db.GetWithRange("key1", "key4")
					require.NoError(t, err)
					assert.Equal(t, []string{"key1", "key2", "key3"}, keys)
					assert.Equal(t, [][]byte{[]byte("val1"), []byte("val2"), []byte("val3")}, vals)

					keys, vals, err = db.GetWithRange("nonexistent", "nonexistent2")
					require.NoError(t, err)
					assert.Nil(t, keys)
					assert.Nil(t, vals)
				})

				t.Run("Prefix", func(t *testing.T) {
					keys, vals, err := db.GetWithPrefix("key")
					require.NoError(t, err)
					assert.Equal(t, []string{"key1", "key2", "key3", "key9"}, keys)
					assert.Equal(t, [][]byte{[]byte("val1"), []byte("val2"), []byte("val3"), []byte("val9")}, vals)

					keys, vals, err = db.GetWithPrefix("nonexistent")
					require.NoError(t, err)
					assert.Nil(t, keys)
					assert.Nil(t, vals)
				})
			})

			t.Run("Delete", func(t *testing.T) {
				setupDatastore(t, db)
				err := db.Delete("key2")
				require.NoError(t, err)
				v, err := db.Get("key2")
				require.NoError(t, err)
				assert.Nil(t, v)

				// No error when deleting nonexistent keys.
				err = db.Delete("nonexistent")
				require.NoError(t, err)

				v, err = db.Get("key1")
				require.NoError(t, err)
				assert.Equal(t, "val1", string(v))
			})

			t.Run("DeleteAll", func(t *testing.T) {
				setupDatastore(t, db)
				err := db.DeleteAll([]string{"key1", "key3", "nonexistent"})
				require.NoError(t, err)
				v, err := db.Get("key1")
				require.NoError(t, err)
				assert.Nil(t, v)

				v, err = db.Get("key2")
				require.NoError(t, err)
				assert.Equal(t, "val2", string(v))
			})

			t.Run("DeletePrefix", func(t *testing.T) {
				setupDatastore(t, db)
				err := db.DeleteWithPrefix("key")

				require.NoError(t, err)
				v, err := db.Get("key1")
				require.NoError(t, err)
				assert.Nil(t, v)

				v, err = db.Get("key2")
				require.NoError(t, err)
				assert.Nil(t, v)

				v, err = db.Get("jam1")
				require.NoError(t, err)
				assert.Equal(t, "neg", string(v))

				// No error when deleting nonexistent keys.
				err = db.DeleteWithPrefix("nonexistent")
				require.NoError(t, err)
			})

			if tc.runTTLTests {
				t.Run("SetWithTTL", func(t *testing.T) {
					now := time.Now()
					ttl := 3 * time.Second

					err := db.SetWithTTL("/timed1", "limited1", ttl)
					require.NoError(t, err)
					// Set and reset TTL
					err = db.SetWithTTL("timed2", "limited2", ttl)
					require.NoError(t, err)
					err = db.SetWithTTL("timed2", "limited2", 1*time.Hour)
					require.NoError(t, err)

					timedOut := time.After(60 * time.Second)
					ticker := time.NewTicker(1 * time.Second)
					defer ticker.Stop()

					for {
						select {
						case <-timedOut:
							// Log but don't fail since this is flaky on CPU constrained
							// environments.
							t.Log("WARNING: set with TTL timed out and key still exists")
							return
						case <-ticker.C:
							v, err := db.Get("/timed1")
							require.NoError(t, err)

							ttlTime, _, err := db.GetWithPrefix("___ttl_time___")
							require.NoError(t, err)

							if time.Since(now) < ttl {
								assert.Equal(t, "limited1", string(v))
								continue
							}

							// Key timed1 might have been deleted but the ttl_timer markers
							// for it seem to still be around. Since these markers should get
							// deleted immediately after, let's wait a tiny bit longer to check.
							if len(ttlTime) > 1 {
								continue
							}

							if v == nil {
								// Key timed1 was deleted some time after TTL passed.

								// Key timed2 should still exist since a longer TTL was set on it.
								v, err = db.Get("timed2")
								require.NoError(t, err)
								assert.Equal(t, "limited2", string(v))

								// Ensure that TTL marker (used only by pebbledb impl) is also gone.
								keys, _, err := db.GetWithPrefix("___ttl___")
								require.NoError(t, err)
								assert.Len(t, keys, 1)
								assert.Equal(t, keys, []string{"___ttl___/timed2"})

								keys, _, err = db.GetWithPrefix("___ttl_time___")
								require.NoError(t, err)
								assert.Len(t, keys, 1)

								return
							}
						}
					}
				})
			}

			err := db.Close()
			assert.NoError(t, err)

			// Calling close repeatedly should be fine
			err = db.Close()
			assert.NoError(t, err)
		})
	}
}

func Fuzz_PebbleDB_SetGetDel(f *testing.F) {
	memFS := vfs.NewMem()
	pbbl, err := pebble.Open("test", &pebble.Options{
		FS: memFS,
	})
	if err != nil {
		f.Fatal("failed to initialize a pebbledb")
	}
	db := pebbledb.New(pbbl, 5*time.Minute)

	f.Add("key", "val")
	f.Fuzz(func(t *testing.T, key string, val string) {
		err := db.Set(key, val)
		assert.NoError(t, err)

		v, err := db.Get(key)
		assert.NoError(t, err)
		assert.Equal(t, val, string(v))

		err = db.Delete(key)
		assert.NoError(t, err)

		v, err = db.Get(key)
		assert.NoError(t, err)
		assert.Nil(t, v)
	})
}

func Fuzz_PebbleDB_SetGetDelPrefix(f *testing.F) {
	memFS := vfs.NewMem()
	pbbl, err := pebble.Open("test", &pebble.Options{
		FS: memFS,
	})
	if err != nil {
		f.Fatal("failed to initialize a pebbledb")
	}
	db := pebbledb.New(pbbl, 5*time.Minute)

	f.Add("pref", "/", "key", "val")
	f.Add(string([]byte{255}), "/", "key", "val")
	f.Add(string([]byte{40, 255}), "", "key", "val")

	f.Fuzz(func(t *testing.T, prefix string, delim string, key string, val string) {
		ub := pebbledb.KeyUpperBound([]byte(prefix))
		if ub == nil {
			_, _, err := db.GetWithPrefix(prefix)
			assert.ErrorContains(t, err, "unsupported prefix")

			err = db.DeleteWithPrefix(prefix)
			assert.ErrorContains(t, err, "unsupported prefix")

			return
		}

		kvs := map[string]string{
			fmt.Sprintf("%s%s%s", prefix, delim, key):                 val,
			fmt.Sprintf("%s%s%s%s", prefix, delim, key, delim):        val,
			fmt.Sprintf("%s%s%s%s%s", prefix, delim, key, delim, key): val,
			fmt.Sprintf("%s%s%s%s%s", prefix, delim, key, delim, val): key,
		}

		for k, v := range kvs {
			err := db.Set(k, v)
			assert.NoError(t, err)
		}

		ks, vs, err := db.GetWithPrefix(prefix)
		fmt.Println(len(prefix), kvs, ks, vs)
		assert.NoError(t, err)
		assert.Equal(t, len(ks), len(vs))
		assert.Equal(t, len(kvs), len(vs))

		for i, k := range ks {
			assert.Equal(t, kvs[k], string(vs[i]))
		}

		err = db.DeleteWithPrefix(prefix)
		assert.NoError(t, err)

		ks, vs, err = db.GetWithPrefix(prefix)
		assert.NoError(t, err)
		assert.Empty(t, ks)
		assert.Empty(t, vs)
	})
}
