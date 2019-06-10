package controllers_test

import (
	"context"
	"fmt"
	"net/url"
	"testing"
	"time"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/embed"
	"github.com/gogo/protobuf/proto"
	"github.com/phayes/freeport"
	log "github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
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

	cleanup := func() {
		e.Close()
		etcdClient.Close()
	}

	return etcdClient, cleanup
}

func TestUpdateEndpoints(t *testing.T) {
	etcdClient, cleanup := setupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(endpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateEndpoints(expectedPb)
	if err != nil {
		t.Fatal("Could not update endpoints.")
	}

	// Check that correct endpoint info is in etcd.
	resp, err := etcdClient.Get(context.Background(), "/endpoints/a_namespace/ijkl")
	if err != nil {
		t.Fatal("Failed to get endpoints.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &metadatapb.Endpoints{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, expectedPb, pb)
}

func TestUpdatePod(t *testing.T) {
	etcdClient, cleanup := setupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(podPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdatePod(expectedPb)
	if err != nil {
		t.Fatal("Could not update pod.")
	}

	// Check that correct pod info is in etcd.
	resp, err := etcdClient.Get(context.Background(), "/pod/a_namespace/ijkl")
	if err != nil {
		t.Fatal("Failed to get pod.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &metadatapb.Pod{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, expectedPb, pb)
}

func TestUpdateService(t *testing.T) {
	etcdClient, cleanup := setupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(servicePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateService(expectedPb)
	if err != nil {
		t.Fatal("Could not update service.")
	}

	// Check that correct service info is in etcd.
	resp, err := etcdClient.Get(context.Background(), "/service/a_namespace/ijkl")
	if err != nil {
		t.Fatal("Failed to get service.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &metadatapb.Service{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, expectedPb, pb)
}
