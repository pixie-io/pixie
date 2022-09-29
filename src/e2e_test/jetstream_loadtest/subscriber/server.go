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

package main

import (
	"math/rand"
	"sync"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/prometheus/client_golang/prometheus"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/e2e_test/jetstream_loadtest/common"
	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/utils"
)

var (
	msgRec = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "px_msg_rec",
		Help: "The number of msg received for the channel",
	}, []string{"channel"})

	msgAckOk = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "px_msg_ack_ok",
		Help: "The number of msg acked for the channel",
	}, []string{"channel"})

	msgAckFail = prometheus.NewCounterVec(prometheus.CounterOpts{
		Name: "px_msg_ack_fail",
		Help: "The number of msg not acked for the channel",
	}, []string{"channel"})

	msgRecTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "TOTAL_px_msg_rec",
		Help: "The total number of msg received",
	})

	msgAckOkTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "TOTAL_px_msg_ack_ok",
		Help: "The total number of msg received and acked",
	})

	msgAckFailTotal = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "TOTAL_px_msg_ack_fail",
		Help: "The total number of msg received and not acked",
	})

	chanSeen = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "px_chan_seen",
		Help: "The number of channels seen",
	})

	chanSubOk = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "px_chan_sub_ok",
		Help: "The number of channels subscribed to",
	})

	chanSubErr = prometheus.NewCounter(prometheus.CounterOpts{
		Name: "px_chan_sub_err",
		Help: "The number of channels where subscription failed",
	})

	startTime = prometheus.NewGauge(prometheus.GaugeOpts{
		Name: "TIME_px_sub_start_time",
		Help: "Start time in unix sec",
	})

	currentTime = prometheus.NewGauge(prometheus.GaugeOpts{
		Name: "TIME_px_sub_current_time",
		Help: "Current time in unix sec",
	})
)

func init() {
	pflag.Float64("failed_ack", 0.02, "The fraction of messages we should randomly fail to ack.")

	prometheus.MustRegister(msgRec)
	prometheus.MustRegister(msgAckOk)
	prometheus.MustRegister(msgAckFail)
	prometheus.MustRegister(msgRecTotal)
	prometheus.MustRegister(msgAckOkTotal)
	prometheus.MustRegister(msgAckFailTotal)
	prometheus.MustRegister(chanSeen)
	prometheus.MustRegister(chanSubOk)
	prometheus.MustRegister(chanSubErr)

	prometheus.MustRegister(startTime)
	prometheus.MustRegister(currentTime)
}

type loadSub struct {
	st   msgbus.Streamer
	seen sync.Map

	done chan struct{}
}

func (s *loadSub) start() {
	startTime.SetToCurrentTime()
	go func() {
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-s.done:
				return
			case <-ticker.C:
				currentTime.SetToCurrentTime()
			}
		}
	}()

	sub, err := s.st.PersistentSubscribe(common.ChannelTrackTopic, "subscriber", func(m msgbus.Msg) {
		chName := m.Data()
		go s.watch(chName)
		_ = m.Ack()
	})
	if err != nil {
		log.WithError(err).Fatal("Failed to start subscriber")
	}

	// Block until done closes.
	<-s.done
	sub.Close()
}

func (s *loadSub) watch(data []byte) {
	chanSeen.Inc()

	cPb := &uuidpb.UUID{}
	err := proto.Unmarshal(data, cPb)
	if err != nil {
		log.WithError(err).Error("failed to unmarshal")
		return
	}
	name, err := utils.UUIDFromProto(cPb)
	if err != nil {
		log.WithError(err).Error("failed to get UUID")
		return
	}
	c := name.String()
	if _, ok := s.seen.LoadOrStore(c, true); ok {
		return
	}

	log.WithField("channel", c).Info("watching channel")
	count := 0
	sub, err := s.st.PersistentSubscribe(common.GetTopicForChannel(c), c+"watcher", func(m msgbus.Msg) {
		msgRec.WithLabelValues(c).Inc()
		msgRecTotal.Inc()
		_ = m.Data()
		if rand.Float64() < viper.GetFloat64("failed_ack") {
			// Don't ack some percentage of messages.
			msgAckFail.WithLabelValues(c).Inc()
			msgAckFailTotal.Inc()
			return
		}
		_ = m.Ack()
		msgAckOk.WithLabelValues(c).Inc()
		msgAckOkTotal.Inc()
		count++
		if count%common.LogEveryN == 0 {
			log.WithField("channel", c).Infof("recd count: %d", count)
		}
	})
	if err != nil {
		chanSubErr.Inc()
		log.WithField("channel", c).WithError(err).Error("Failed to subscribe to channel")
		return
	}
	chanSubOk.Inc()
	// Block until done closes.
	<-s.done
	sub.Close()
}

func main() {
	st := common.SetupJetStream(false)
	sub := loadSub{
		st:   st,
		done: make(chan struct{}),
	}

	sub.start()
}
