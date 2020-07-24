package kvstore_test

import (
	"context"
	"os"
	"testing"
	"time"

	"github.com/coreos/etcd/clientv3"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
)

var etcdClient *clientv3.Client

func TestEtcdStore_Get(t *testing.T) {
	_, err := etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}

	// Add items into etcd that we can get.
	_, err = etcdClient.Put(context.Background(), "/abcd", "hello")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}

	e := kvstore.NewEtcdStore(etcdClient)
	val, err := e.Get("/abcd")
	assert.Nil(t, err)
	assert.Equal(t, []byte("hello"), val)

	val, err = e.Get("/test")
	assert.Nil(t, err)
	assert.Equal(t, []byte(nil), val)
}

func TestEtcdStore_SetAll(t *testing.T) {
	_, err := etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}

	_, err = etcdClient.Put(context.Background(), "/existingKey", "xyz")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}

	e := kvstore.NewEtcdStore(etcdClient)
	setArr := []kvstore.TTLKeyValue{
		kvstore.TTLKeyValue{
			Key:    "/abc",
			Value:  []byte("1234"),
			Expire: false,
			TTL:    0,
		},
		kvstore.TTLKeyValue{
			Key:    "/def",
			Value:  []byte("5678"),
			Expire: true,
			TTL:    10800,
		},
		kvstore.TTLKeyValue{
			Key:    "/existingKey",
			Value:  []byte("abcd"),
			Expire: true,
			TTL:    -10800,
		},
		kvstore.TTLKeyValue{
			Key:    "/nonExistentKey",
			Value:  []byte("efgh"),
			Expire: true,
			TTL:    -10800,
		},
	}

	err = e.SetAll(setArr)
	assert.Nil(t, err)

	kv, err := etcdClient.Get(context.Background(), "/abc")
	if err != nil {
		t.Fatal("Could not get value from etcd")
	}
	assert.Nil(t, err)
	assert.Equal(t, "1234", string(kv.Kvs[0].Value))

	kv, err = etcdClient.Get(context.Background(), "/def")
	if err != nil {
		t.Fatal("Could not get value from etcd")
	}
	assert.Nil(t, err)
	assert.Equal(t, "5678", string(kv.Kvs[0].Value))

	kv, err = etcdClient.Get(context.Background(), "/existingKey")
	if err != nil {
		t.Fatal("Could not get value from etcd")
	}
	assert.Nil(t, err)
	assert.Equal(t, 0, len(kv.Kvs))

	kv, err = etcdClient.Get(context.Background(), "/nonExistentKey")
	if err != nil {
		t.Fatal("Could not get value from etcd")
	}
	assert.Nil(t, err)
	assert.Equal(t, 0, len(kv.Kvs))
}

func TestEtcdStore_GetWithPrefix(t *testing.T) {
	_, err := etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}

	// Add items into etcd that we can get.
	_, err = etcdClient.Put(context.Background(), "/abcd", "hello")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/aefg", "hola")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/ahij", "ciao")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/bklm", "bonjour")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}

	e := kvstore.NewEtcdStore(etcdClient)
	keys, vals, err := e.GetWithPrefix("/a")

	assert.Nil(t, err)
	assert.Equal(t, 3, len(keys))
	assert.Equal(t, "/abcd", keys[0])
	assert.Equal(t, "/aefg", keys[1])
	assert.Equal(t, "/ahij", keys[2])
	assert.Equal(t, 3, len(vals))
	assert.Equal(t, []byte("hello"), vals[0])
	assert.Equal(t, []byte("hola"), vals[1])
	assert.Equal(t, []byte("ciao"), vals[2])

	keys, vals, err = e.GetWithPrefix("/c")
	assert.Nil(t, err)
	assert.Equal(t, 0, len(keys))
	assert.Equal(t, 0, len(vals))
}

func TestEtcdStore_GetAll(t *testing.T) {
	_, err := etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}

	// Add items into etcd that we can get.
	_, err = etcdClient.Put(context.Background(), "/abcd", "hello")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/aefg", "hola")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/ahij", "ciao")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/bklm", "bonjour")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}

	e := kvstore.NewEtcdStore(etcdClient)
	vals, err := e.GetAll([]string{"/abcd", "/bklm", "/aefg", "/doesntexist"})
	assert.Nil(t, err)
	assert.Equal(t, 4, len(vals))
}

func TestEtcdStore_DeleteWithPrefix(t *testing.T) {
	_, err := etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}

	// Add items into etcd that we can get.
	_, err = etcdClient.Put(context.Background(), "/abcd", "hello")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/aefg", "hola")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/ahij", "ciao")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/bklm", "bonjour")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}

	e := kvstore.NewEtcdStore(etcdClient)
	err = e.DeleteWithPrefix("/a")
	assert.Nil(t, err)

	kv, err := etcdClient.Get(context.Background(), "/abcd")
	if err != nil {
		t.Fatal("Could not get value from etcd")
	}
	assert.Nil(t, err)
	assert.Equal(t, 0, len(kv.Kvs))

	kv, err = etcdClient.Get(context.Background(), "/aefg")
	if err != nil {
		t.Fatal("Could not get value from etcd")
	}
	assert.Nil(t, err)
	assert.Equal(t, 0, len(kv.Kvs))

	kv, err = etcdClient.Get(context.Background(), "/ahij")
	if err != nil {
		t.Fatal("Could not get value from etcd")
	}
	assert.Nil(t, err)
	assert.Equal(t, 0, len(kv.Kvs))

	kv, err = etcdClient.Get(context.Background(), "/bklm")
	if err != nil {
		t.Fatal("Could not get value from etcd")
	}
	assert.Nil(t, err)
	assert.Equal(t, 1, len(kv.Kvs))
}

func TestEtcdStore_GetWithRange(t *testing.T) {
	_, err := etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}

	// Add items into etcd that we can get.
	_, err = etcdClient.Put(context.Background(), "/1", "hello")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/2", "hola")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/3", "ciao")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/4", "bonjour")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}
	_, err = etcdClient.Put(context.Background(), "/5", "hi")
	if err != nil {
		t.Fatal("Could not put value into etcd from etcd")
	}

	e := kvstore.NewEtcdStore(etcdClient)
	keys, vals, err := e.GetWithRange("/2", "/4")
	assert.Nil(t, err)
	assert.Equal(t, 2, len(vals))
	assert.Equal(t, 2, len(keys))
	assert.Equal(t, "/2", keys[0])
	assert.Equal(t, []byte("hola"), vals[0])
	assert.Equal(t, "/3", keys[1])
	assert.Equal(t, []byte("ciao"), vals[1])
}

func TestEtcd_Watcher(t *testing.T) {
	e := kvstore.NewEtcdStore(etcdClient)

	eventCh, quitCh := e.WatchKeyEvents("/")
	defer func() {
		quitCh <- true
	}()

	setArr := []kvstore.TTLKeyValue{
		kvstore.TTLKeyValue{
			Key:    "/def",
			Value:  []byte("5678"),
			Expire: true,
			TTL:    5,
		},
	}

	_ = e.SetAll(setArr)

	for {
		select {
		case event := <-eventCh:
			assert.Equal(t, kvstore.EventTypePut, event.EventType)
			assert.Equal(t, "/def", event.Key)
			return
		case <-time.After(2 * time.Second):
			t.Fatal("Timed out waiting for etcd event")
		}
	}
}

func TestMain(m *testing.M) {
	c, cleanup := testingutils.SetupEtcd()
	etcdClient = c
	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}
