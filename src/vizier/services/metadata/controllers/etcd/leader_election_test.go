package etcd_test

import (
	"context"
	"testing"
	"time"

	"github.com/coreos/etcd/clientv3/concurrency"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
)

func TestLeaderElection(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	sess, err := concurrency.NewSession(etcdClient, concurrency.WithContext(context.Background()))
	if err != nil {
		t.Fatal("Could not create new session for etcd")
	}

	l := etcd.NewLeaderElectionWithPollTime(sess, 5*time.Second)

	// First campaign attempt should succeed.
	err = l.Campaign()
	assert.Nil(t, err)

	isLeader, err := l.IsLeader()
	assert.Equal(t, true, isLeader)

	// Campaigning when the instance is already the leader should succeed.
	err = l.Campaign()
	assert.Nil(t, err)

	isLeader, err = l.IsLeader()
	assert.Equal(t, true, isLeader)

	sess2, err := concurrency.NewSession(etcdClient, concurrency.WithContext(context.Background()))
	if err != nil {
		t.Fatal("Could not create new session for etcd")
	}

	l2 := etcd.NewLeaderElectionWithPollTime(sess2, 5*time.Second)

	// Campaigning when another instance is already the leader should fail.
	err = l2.Campaign()
	assert.NotNil(t, err)

	isLeader, err = l2.IsLeader()
	assert.Equal(t, false, isLeader)
}

func TestLeaderElection_Stop(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	sess, err := concurrency.NewSession(etcdClient, concurrency.WithContext(context.Background()))
	if err != nil {
		t.Fatal("Could not create new session for etcd")
	}

	l := etcd.NewLeaderElectionWithPollTime(sess, 5*time.Second)

	// First campaign attempt should succeed.
	err = l.Campaign()
	assert.Nil(t, err)

	isLeader, err := l.IsLeader()
	assert.Equal(t, true, isLeader)

	// Campaigning when the instance is already the leader should succeed.
	err = l.Campaign()
	assert.Nil(t, err)

	isLeader, err = l.IsLeader()
	assert.Equal(t, true, isLeader)

	sess2, err := concurrency.NewSession(etcdClient, concurrency.WithContext(context.Background()))
	if err != nil {
		t.Fatal("Could not create new session for etcd")
	}

	l2 := etcd.NewLeaderElectionWithPollTime(sess2, 5*time.Second)

	// Campaigning when another instance is already the leader should fail.
	err = l2.Campaign()
	assert.NotNil(t, err)

	isLeader, err = l2.IsLeader()
	assert.Equal(t, false, isLeader)

	err = l.Stop()
	assert.Nil(t, err)

	// Stop should resign the first instance as leader.
	isLeader, err = l.IsLeader()
	assert.Equal(t, false, isLeader)

	// Campaigning should be successful since there is no current leader.
	err = l2.Campaign()
	assert.Nil(t, err)

	isLeader, err = l2.IsLeader()
	assert.Equal(t, true, isLeader)

}
