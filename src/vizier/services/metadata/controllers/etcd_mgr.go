package controllers

import (
	"context"
	"sync/atomic"
	"time"

	v3 "github.com/coreos/etcd/clientv3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
)

const (
	defragBytes     = 50000000      // Begin defragging @ 500MB.
	defragFrequency = 1 * time.Hour // The minimum amount of times we should have inbetween defrags.
)

// EtcdManager manages state for the given etcd instance.
type EtcdManager struct {
	client     *v3.Client
	defragging atomic.Value
	lastDefrag time.Time

	quitCh chan bool
	timer  *time.Ticker
}

// NewEtcdManager creates a new etcd manager.
func NewEtcdManager(client *v3.Client) *EtcdManager {
	return &EtcdManager{
		client: client,
		quitCh: make(chan bool),
	}
}

// Run periodically checks the size of the etcd db and defrags if necessary.
func (m *EtcdManager) Run() {
	m.defragging.Store(false)
	m.timer = time.NewTicker(5 * time.Minute)

	go func() {
		// Periodically check etcd state, and its memory usage.
		for {
			select {
			case _, ok := <-m.quitCh:
				if !ok {
					return
				}
			case <-m.timer.C:
				resp, err := m.client.Status(context.Background(), viper.GetString("md_etcd_server"))
				if err != nil {
					log.WithError(err).Error("Failed to get etcd members")
					continue
				}
				log.WithField("dbSizeBytes", resp.DbSize).Info("Etcd state")

				now := time.Now()
				elapsed := now.Sub(m.lastDefrag)

				if resp.DbSize > defragBytes && elapsed > defragFrequency {
					log.Info("Starting defrag")
					m.defragging.Store(true)

					defragResp, err := m.client.Defragment(context.Background(), viper.GetString("md_etcd_server"))
					if err != nil {
						log.WithError(err).Error("Failed to defrag")
					} else {
						log.WithField("resp", defragResp).Info("Finished defragging")
					}
					m.lastDefrag = time.Now()

					m.defragging.Store(false)
				}
			}
		}
	}()
}

// Stop stops the etcd manager from periodically checking and defragging etcd.
func (m *EtcdManager) Stop() {
	m.timer.Stop()
	close(m.quitCh)
}

// IsDefragging returns whether etcd is currently running a defrag.
func (m *EtcdManager) IsDefragging() bool {
	return m.defragging.Load().(bool)
}
