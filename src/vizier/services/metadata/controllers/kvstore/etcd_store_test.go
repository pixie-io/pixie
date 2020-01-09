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

	e := kvstore.NewEtcdStore(etcdClient)
	setMap := make(map[string]kvstore.Entry)
	setMap["/abc"] = kvstore.Entry{
		ExpiresAt: time.Time{},
		Value:     []byte("1234"),
	}
	setMap["/def"] = kvstore.Entry{
		ExpiresAt: time.Now().Add(time.Hour * 3),
		Value:     []byte("5678"),
	}

	err = e.SetAll(&setMap)
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

func TestMain(m *testing.M) {
	c, cleanup := testingutils.SetupEtcd()
	etcdClient = c
	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}
