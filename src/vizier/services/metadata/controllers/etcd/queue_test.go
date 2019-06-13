package etcd_test

import (
	"context"
	"fmt"
	"net/url"
	"testing"
	"time"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/embed"
	"github.com/phayes/freeport"
	log "github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
)

func setupEtcd(t *testing.T) (*clientv3.Client, func()) {
	// Find available port.
	port, err := freeport.GetFreePort()
	if err != nil {
		t.Fatal("Could not find free port")
	}

	// Start up etcd server.
	cfg := embed.NewConfig()
	cfg.Dir = "default.etcd"
	lcURL, err := url.Parse(fmt.Sprintf("http://localhost:%d", port))
	if err != nil {
		t.Fatal("Could not parse URL.")
	}
	cfg.LCUrls = []url.URL{*lcURL}
	e, err := embed.StartEtcd(cfg)
	if err != nil {
		t.Fatal("Could not start etcd server.")
	}

	select {
	case <-e.Server.ReadyNotify():
		log.Info("Server is ready.")
	case <-time.After(60 * time.Second):
		e.Server.Stop()
		t.Fatal("Server took too long to start, stopping server.")
	}

	// Add some existing agent data into etcd.
	etcdClient, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{fmt.Sprintf("http://localhost:%d", port)},
		DialTimeout: 5 * time.Second,
	})

	_, err = etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}

	cleanup := func() {
		e.Close()
		etcdClient.Close()
	}

	return etcdClient, cleanup
}

func TestEnqueue(t *testing.T) {
	etcdClient, cleanup := setupEtcd(t)
	defer cleanup()

	q := etcd.NewQueue(etcdClient, "test")

	err := q.Enqueue("abcd")
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	err = q.Enqueue("efgh")
	assert.Nil(t, err)

	// First revision should still be key with value abcd.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithFirstRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "abcd", string(resp.Kvs[0].Value))

	// Last revision should be key with value efgh.
	resp, err = etcdClient.Get(context.Background(), "test", clientv3.WithLastRev()...)
	if err != nil {
		t.Fatal("Failed to get item in queue.")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, "efgh", string(resp.Kvs[0].Value))
}

func TestDequeueMultiple(t *testing.T) {
	etcdClient, cleanup := setupEtcd(t)
	defer cleanup()

	q := etcd.NewQueue(etcdClient, "test")

	err := q.Enqueue("abcd")
	assert.Nil(t, err)

	err = q.Enqueue("efgh")
	assert.Nil(t, err)

	err = q.Enqueue("ijkl")
	assert.Nil(t, err)

	resp, err := q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "abcd", resp)

	resp, err = q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "efgh", resp)

	resp, err = q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "ijkl", resp)

	resp, err = q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "", resp)
}

func TestDequeueEmpty(t *testing.T) {
	etcdClient, cleanup := setupEtcd(t)
	defer cleanup()

	q := etcd.NewQueue(etcdClient, "test")

	resp, err := q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "", resp)
}
