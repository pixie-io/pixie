package testingutils

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
)

// SetupEtcd starts up an embedded etcd server on some free ports.
func SetupEtcd(t *testing.T) (*clientv3.Client, func()) {
	// Find available port.
	clientPort, err := freeport.GetFreePort()
	if err != nil {
		t.Fatal("Could not find free port")
	}

	peerPort, err := freeport.GetFreePort()
	if err != nil {
		t.Fatal("Could not find free port")
	}

	// Start up etcd server.
	cfg := embed.NewConfig()
	cfg.Dir = "default.etcd"
	lcURL, err := url.Parse(fmt.Sprintf("http://localhost:%d", clientPort))
	if err != nil {
		t.Fatal("Could not parse URL.")
	}
	lpURL, err := url.Parse(fmt.Sprintf("http://localhost:%d", peerPort))
	if err != nil {
		t.Fatal("Could not parse URL.")
	}
	cfg.LCUrls = []url.URL{*lcURL}
	cfg.LPUrls = []url.URL{*lpURL}
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
		Endpoints:   []string{fmt.Sprintf("http://localhost:%d", clientPort)},
		DialTimeout: 5 * time.Second,
	})

	if err != nil {
		t.Fatal("Failed to connect to etcd.")
	}

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
