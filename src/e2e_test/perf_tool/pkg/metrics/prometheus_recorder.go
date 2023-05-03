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

package metrics

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"sync"
	"time"

	"github.com/cenkalti/backoff/v4"
	"github.com/gogo/protobuf/types"
	io_prometheus_client "github.com/prometheus/client_model/go"
	"github.com/prometheus/common/expfmt"
	log "github.com/sirupsen/logrus"
	"golang.org/x/sync/errgroup"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/tools/portforward"
	"k8s.io/client-go/transport/spdy"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
)

type prometheusRecorderImpl struct {
	clusterCtx *cluster.Context
	spec       *experimentpb.PrometheusScrapeSpec
	eg         *errgroup.Group
	resultCh   chan<- *ResultRow

	wg     sync.WaitGroup
	stopCh chan struct{}
	fws    []*forwarder
}

func (r *prometheusRecorderImpl) Start(ctx context.Context) error {
	if err := r.startPortForward(ctx); err != nil {
		return err
	}
	r.stopCh = make(chan struct{})
	r.wg.Add(1)
	r.eg.Go(func() error {
		defer r.wg.Done()
		err := r.run()
		if err != nil {
			err = fmt.Errorf("error recording prometheus metrics: %w", err)
		}
		return err
	})
	return nil
}

func (r *prometheusRecorderImpl) Close() {
	if r.stopCh != nil {
		close(r.stopCh)
		r.wg.Wait()
		r.stopCh = nil
	}
	for _, fw := range r.fws {
		fw.Close()
	}
}

func (r *prometheusRecorderImpl) run() error {
	d, err := types.DurationFromProto(r.spec.ScrapePeriod)
	if err != nil {
		return err
	}
	t := time.NewTicker(d)
	defer t.Stop()
	for {
		select {
		case <-r.stopCh:
			return nil
		case <-t.C:
			if err := r.scrape(); err != nil {
				return err
			}
		}
	}
}

func (r *prometheusRecorderImpl) scrape() error {
	eg := errgroup.Group{}
	for _, fw := range r.fws {
		eg.Go(r.scrapeFunc(fw))
	}
	return eg.Wait()
}

func (r *prometheusRecorderImpl) scrapeFunc(fw *forwarder) func() error {
	op := func() error {
		resp, err := http.Get(fmt.Sprintf("http://localhost:%d/metrics", fw.localPort))
		if err != nil {
			return err
		}
		ts := time.Now()
		metrics, err := (&expfmt.TextParser{}).TextToMetricFamilies(resp.Body)
		if err != nil {
			return backoff.Permanent(err)
		}
		for _, mf := range metrics {
			name, ok := r.spec.MetricNames[mf.GetName()]
			if !ok {
				// Only collect metrics named in the spec.
				continue
			}
			// Only collect counters and gauges for now.
			if !(mf.GetType() == io_prometheus_client.MetricType_COUNTER || mf.GetType() == io_prometheus_client.MetricType_GAUGE) {
				continue
			}
			for _, m := range mf.GetMetric() {
				row := newResultRow()
				row.Name = name
				row.Timestamp = ts
				for _, lp := range m.GetLabel() {
					row.Tags[lp.GetName()] = lp.GetValue()
				}
				row.Tags["pod"] = fw.podName
				switch mf.GetType() {
				case io_prometheus_client.MetricType_COUNTER:
					row.Value = m.GetCounter().GetValue()
				case io_prometheus_client.MetricType_GAUGE:
					row.Value = m.GetGauge().GetValue()
				}
				r.resultCh <- row
			}
		}

		return nil
	}
	bo := backoff.NewExponentialBackOff()
	bo.MaxElapsedTime = 20 * time.Second

	return func() error { return backoff.Retry(op, bo) }
}

func (r *prometheusRecorderImpl) startPortForward(ctx context.Context) error {
	pl, err := r.clusterCtx.Clientset().CoreV1().Pods(r.spec.Namespace).List(ctx, metav1.ListOptions{
		LabelSelector: metav1.FormatLabelSelector(&metav1.LabelSelector{
			MatchLabels: map[string]string{
				r.spec.MatchLabelKey: r.spec.MatchLabelValue,
			},
		}),
	})
	if err != nil {
		return err
	}

	fws := make(chan *forwarder, len(pl.Items))
	eg := errgroup.Group{}
	for _, pod := range pl.Items {
		name := pod.Name
		eg.Go(func() error {
			fw, err := newForwarder(ctx, r.clusterCtx, r.spec.Namespace, name, uint16(r.spec.Port))
			if err != nil {
				return err
			}
			if err := fw.Start(); err != nil {
				return err
			}
			fws <- fw
			return nil
		})
	}

	if err := eg.Wait(); err != nil {
		log.WithError(err).Error("error starting port forwarders for prometheus scraping")
		close(fws)
		for fw := range fws {
			fw.Close()
		}
		return err
	}
	close(fws)
	r.fws = make([]*forwarder, 0)
	for fw := range fws {
		r.fws = append(r.fws, fw)
	}

	return nil
}

type forwarder struct {
	localPort uint16
	pf        *portforward.PortForwarder
	stopCh    chan struct{}
	readyCh   chan struct{}
	errCh     chan error
	podName   string

	wg sync.WaitGroup
}

func newForwarder(ctx context.Context, clusterCtx *cluster.Context, ns string, pod string, remotePort uint16) (*forwarder, error) {
	p, err := clusterCtx.Clientset().CoreV1().Pods(ns).Get(ctx, pod, metav1.GetOptions{})
	if err != nil {
		return nil, err
	}
	req := clusterCtx.Clientset().CoreV1().RESTClient().
		Post().
		Resource("pods").
		Namespace(ns).
		Name(p.Name).
		SubResource("portforward")

	transport, upgrader, err := spdy.RoundTripperFor(clusterCtx.RestConfig())
	if err != nil {
		return nil, err
	}
	dialer := spdy.NewDialer(upgrader, &http.Client{Transport: transport}, "POST", req.URL())

	fw := &forwarder{
		podName: pod,
		stopCh:  make(chan struct{}),
		errCh:   make(chan error),
		readyCh: make(chan struct{}),
	}

	ports := []string{fmt.Sprintf("0:%d", remotePort)}

	pf, err := portforward.New(dialer, ports, fw.stopCh, fw.readyCh, io.Discard, os.Stderr)
	if err != nil {
		return nil, err
	}
	fw.pf = pf
	return fw, nil
}

func (fw *forwarder) Start() error {
	fw.wg.Add(1)
	go fw.run()
	select {
	case err := <-fw.errCh:
		return err
	case <-fw.readyCh:
	}
	fwPorts, err := fw.pf.GetPorts()
	if err != nil {
		fw.pf.Close()
		close(fw.stopCh)
		return err
	}
	if len(fwPorts) != 1 {
		fw.pf.Close()
		close(fw.stopCh)
		return errors.New("couldn't get local port from port forwarder")
	}
	fw.localPort = fwPorts[0].Local
	return nil
}

func (fw *forwarder) run() {
	defer fw.wg.Done()
	err := fw.pf.ForwardPorts()
	if err != nil {
		fw.errCh <- err
	}
	close(fw.errCh)
}

func (fw *forwarder) Close() {
	if fw.stopCh == nil {
		return
	}
	fw.pf.Close()
	close(fw.stopCh)
	defer func() { fw.stopCh = nil }()
	fw.wg.Wait()
}
