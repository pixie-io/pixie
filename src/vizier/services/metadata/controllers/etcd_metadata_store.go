package controllers

import (
	"context"
	"errors"
	"path"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/clientv3/concurrency"
	log "github.com/sirupsen/logrus"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
)

// EtcdMetadataStore is the implementation of our metadata store in etcd.
type EtcdMetadataStore struct {
	client *clientv3.Client
}

// NewEtcdMetadataStore creates a new etcd metadata store.
func NewEtcdMetadataStore(client *clientv3.Client) (*EtcdMetadataStore, error) {
	mds := &EtcdMetadataStore{
		client: client,
	}

	return mds, nil
}

// UpdateEndpoints adds or updates the given endpoint in the metadata store.
func (mds *EtcdMetadataStore) UpdateEndpoints(e *metadatapb.Endpoints) error {
	val, err := e.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getEndpointKey(e)

	return mds.updateValue(key, string(val))
}

func getNamespaceFromMetadata(md *metadatapb.ObjectMetadata) string {
	ns := md.Namespace
	if ns == "" {
		ns = "default"
	}
	return ns
}

func getEndpointKey(e *metadatapb.Endpoints) string {
	return path.Join("/", "endpoints", getNamespaceFromMetadata(e.Metadata), e.Metadata.Uid)
}

// UpdatePod adds or updates the given pod in the metadata store.
func (mds *EtcdMetadataStore) UpdatePod(p *metadatapb.Pod) error {
	val, err := p.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getPodKey(p)

	return mds.updateValue(key, string(val))
}

func getPodKey(e *metadatapb.Pod) string {
	return path.Join("/", "pod", getNamespaceFromMetadata(e.Metadata), e.Metadata.Uid)
}

// UpdateService adds or updates the given service in the metadata store.
func (mds *EtcdMetadataStore) UpdateService(s *metadatapb.Service) error {
	val, err := s.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getServiceKey(s)

	return mds.updateValue(key, string(val))
}

func getServiceKey(e *metadatapb.Service) string {
	return path.Join("/", "service", getNamespaceFromMetadata(e.Metadata), e.Metadata.Uid)
}

func (mds *EtcdMetadataStore) updateValue(key string, value string) error {
	ctx := context.Background()
	ss, err := concurrency.NewSession(mds.client, concurrency.WithContext(ctx))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session")
	}
	defer ss.Close()

	mu := concurrency.NewMutex(ss, key)
	mu.Lock(ctx)
	defer mu.Unlock(ctx)
	_, err = mds.client.Put(context.Background(), key, value)
	if err != nil {
		return errors.New("Unable to update etcd")
	}

	return nil
}
