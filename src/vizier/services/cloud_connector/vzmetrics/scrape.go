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

package vzmetrics

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/utils/shared/k8s"
	"px.dev/pixie/src/vizier/messages/messagespb"
)

// Scraper is the interface for the metrics scraper that periodically scrapes metrics from the /metrics endpoint on each of the services provided to it.
type Scraper interface {
	// Run starts the metrics scraper.
	Run()
	// Stop the metrics scraper.
	Stop()
	// MetricsChannel gets the output channel for metrics scraped by the scraper.
	MetricsChannel() <-chan *messagespb.MetricsMessage
}

const (
	metricsPath          = "/metrics"
	scrapeAnnotationName = "px.dev/metrics_scrape"
	portAnnotationName   = "px.dev/metrics_port"
)

type scraperImpl struct {
	namespace    string
	period       time.Duration
	metricsCh    chan *messagespb.MetricsMessage
	httpClient   *http.Client
	k8sClientset *kubernetes.Clientset
	quitCh       chan bool
}

// NewScraper returns a new metrics scraper with the given scraping period.
func NewScraper(namespace string, period time.Duration) Scraper {
	tlsCACert := viper.GetString("tls_ca_cert")
	certPool := x509.NewCertPool()
	ca, err := os.ReadFile(tlsCACert)
	if err != nil {
		log.WithError(err).Fatal("failed to read CA cert.")
	}
	if ok := certPool.AppendCertsFromPEM(ca); !ok {
		log.Fatal("failed to append cert to cert pool.")
	}

	tr := http.DefaultTransport.(*http.Transport).Clone()
	tr.TLSClientConfig = &tls.Config{RootCAs: certPool}
	client := &http.Client{Transport: tr}

	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		log.WithError(err).Fatal("Unable to get incluster kubeconfig")
	}
	clientset := k8s.GetClientset(kubeConfig)

	return &scraperImpl{
		namespace:    namespace,
		period:       period,
		metricsCh:    make(chan *messagespb.MetricsMessage, 128),
		httpClient:   client,
		k8sClientset: clientset,
		quitCh:       make(chan bool),
	}
}

func (s *scraperImpl) MetricsChannel() <-chan *messagespb.MetricsMessage {
	return s.metricsCh
}

type endpoint struct {
	reqPath string
	podName string
}

func (s *scraperImpl) getEndpointsToScrape() ([]endpoint, error) {
	pods, err := s.k8sClientset.CoreV1().Pods(s.namespace).List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return []endpoint{}, err
	}
	endpoints := make([]endpoint, 0)
	for _, p := range pods.Items {
		if p.ObjectMeta.Annotations[scrapeAnnotationName] != "true" {
			continue
		}
		podName := p.ObjectMeta.Name
		port, ok := p.ObjectMeta.Annotations[portAnnotationName]
		if !ok {
			log.WithField("pod name", podName).Warnf("Pod with %s annotation but no %s annotation which is required for metrics scraping", scrapeAnnotationName, portAnnotationName)
			continue
		}

		// Use k8s pod DNS format instead of just the pod IP, so that the cert is valid.
		host := k8s.GetPodAddr(p)

		u := url.URL{
			Scheme: "https",
			Host:   net.JoinHostPort(host, port),
			Path:   metricsPath,
		}
		endpoints = append(endpoints, endpoint{
			reqPath: u.String(),
			podName: podName,
		})
	}
	return endpoints, nil
}

func (s *scraperImpl) scrapeMetrics() {
	endpoints, err := s.getEndpointsToScrape()
	if err != nil {
		log.WithError(err).Error("failed to get metric endpoints to scrape")
		return
	}

	for _, e := range endpoints {
		resp, err := s.httpClient.Get(e.reqPath)
		if err != nil {
			log.WithError(err).WithField("endpoint", e).Error("failed to scrape metrics")
			continue
		}
		defer resp.Body.Close()
		b, err := io.ReadAll(resp.Body)
		if err != nil {
			log.WithError(err).WithField("endpoint", e).Error("failed to read metrics body")
			continue
		}
		s.metricsCh <- &messagespb.MetricsMessage{
			PromMetricsText: string(b),
			PodName:         e.podName,
		}
	}
}

func (s *scraperImpl) Run() {
	t := time.NewTicker(s.period)
	defer t.Stop()

	for {
		select {
		case <-s.quitCh:
			return
		case <-t.C:
			s.scrapeMetrics()
		}
	}
}

func (s *scraperImpl) Stop() {
	s.quitCh <- true
}
