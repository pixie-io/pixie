package etcd

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/coreos/etcd/clientv3/concurrency"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
)

var electionKey = "/election"
var electionPollTime = 1 * time.Second

// LeaderElection is an object which tracks an election in etcd.
type LeaderElection struct {
	id       uuid.UUID
	session  *concurrency.Session
	election *concurrency.Election
	quitCh   chan bool
	pollTime time.Duration
}

// NewLeaderElection starts a new leader election.
func NewLeaderElection(session *concurrency.Session) *LeaderElection {
	return NewLeaderElectionWithPollTime(session, electionPollTime)
}

// NewLeaderElectionWithPollTime starst a new leader election with the given poll time.
func NewLeaderElectionWithPollTime(session *concurrency.Session, pollTime time.Duration) *LeaderElection {
	id := uuid.NewV4()
	election := concurrency.NewElection(session, electionKey)
	quitCh := make(chan bool)

	return &LeaderElection{id, session, election, quitCh, pollTime}
}

// RunElection starts a loop where the leaderElection actively attempts to become leader.
func (l *LeaderElection) RunElection() {
	for {
		select {
		case <-l.quitCh:
			return
		default:
			err := l.Campaign()
			if err == nil {
				log.Info(fmt.Sprintf("%s is currently leader", l.id.String()))
			}
			time.Sleep(l.pollTime)
		}
	}
}

// Campaign attempts to elect the current instance as leader.
func (l *LeaderElection) Campaign() error {
	ctx, cancel := context.WithTimeout(context.Background(), l.pollTime/2)
	defer cancel()
	return l.election.Campaign(ctx, l.id.String())
}

// IsLeader returns whether this leaderElection instance is currently the leader.
func (l *LeaderElection) IsLeader() (bool, error) {
	ctx := context.Background()
	resp, err := l.election.Leader(ctx)
	if err != nil {
		return false, err
	}
	if len(resp.Kvs) == 0 {
		return false, errors.New("Get leader failed")
	}
	return fmt.Sprintf("%s", resp.Kvs[0].Value) == l.id.String(), nil
}

// Stop stops any further campaign attempts and resigns as leader.
func (l *LeaderElection) Stop() error {
	ctx := context.Background()
	close(l.quitCh)
	return l.election.Resign(ctx)
}
