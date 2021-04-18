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

package cli_e2e_test

import (
	"bytes"
	"encoding/json"
	"errors"
	"flag"
	"io"
	"os"
	"os/exec"
	"testing"

	"github.com/bazelbuild/rules_go/go/tools/bazel"
	log "github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"
	"golang.org/x/sync/errgroup"
)

var testDisableList = []string{
	"px/http2_data",
	"pixielabs/payment_trace_vis",
	"pixielabs/metadata_heartbeat",
	"pixielabs/sock_shop_payment_trace",
	"pixielabs/metadata_send_nats_heartbeat",
	"boutique/checkout_trace",
	"pixielabs/etcd_get_trace",
}

var stressTestDisableList = []string{
	"px/http2_data",
	"pixielabs/payment_trace_vis",
	"pixielabs/metadata_heartbeat",
	"pixielabs/sock_shop_payment_trace",
	"pixielabs/metadata_send_nats_heartbeat",
	"boutique/checkout_trace",
	"pixielabs/etcd_get_trace",
}

type scriptInfo struct {
	Name string `json:"Name"`
}

var (
	pxCLI             = flag.String("px_cli", "", "The path to the pixie cli")
	stressRepeat      = flag.Int("stress_repeat", 1000, "number of times to repeat stress test")
	stressMaxParallel = flag.Int("stress_parallel", 50, "number of scripts to run in parallel. "+
		"Numbers larger than 50 might cause timeouts because of Kelvin thread pool.")
	allClusters = flag.Bool("all-clusters", false, "run script across all clusters")
	clusterID   = flag.String("c", "", "run only on selected cluster ID")
)

func mustCLIPath() string {
	cli, err := bazel.Runfile(*pxCLI)
	if err != nil {
		log.WithError(err).Fatal("Failed to locate CLI")
	}
	return cli
}

func cliExecStdoutBytes(t *testing.T, args ...string) (*bytes.Buffer, error) {
	cli := mustCLIPath()

	c := exec.Command(cli, args...)
	c.Stderr = os.Stderr
	r, err := c.StdoutPipe()
	if err != nil {
		t.Fatal(err)
	}
	defer r.Close()

	var buf bytes.Buffer
	go func() {
		_, err := io.Copy(&buf, r)
		if err != nil {
			log.WithError(err).Error("Failed to copy")
		}
	}()

	if err := c.Run(); err != nil {
		return nil, err
	}

	return &buf, err
}

func mustGetScriptList(t *testing.T) []scriptInfo {
	// Get script list in JSON format.
	b, err := cliExecStdoutBytes(t, "script", "list", "-o", "json")
	if err != nil {
		t.Fatalf("Failed to get script list: %+v", err)
	}

	scripts := make([]scriptInfo, 0)
	dec := json.NewDecoder(b)
	for {
		var s scriptInfo
		err := dec.Decode(&s)
		if err != nil {
			if err == io.EOF {
				break
			}
			t.Fatalf("Failed to decode script list: %+v", err)
		}
		scripts = append(scripts, s)
	}
	return scripts
}

func TestCLIE2E_AllScripts(t *testing.T) {
	scripts := mustGetScriptList(t)
	// Run each script once.
	for _, s := range scripts {
		t.Run(s.Name, func(t *testing.T) {
			if containsStr(testDisableList, s.Name) {
				t.Skip()
				return
			}
			var b *bytes.Buffer
			var err error
			switch {
			case *allClusters:
				b, err = cliExecStdoutBytes(t, "run", s.Name, "--all-clusters", "-o", "json")
			case *clusterID != "":
				b, err = cliExecStdoutBytes(t, "run", s.Name, "-c", *clusterID, "-o", "json")
			default:
				t.Fatalf("Either --all-clusters or -c <cluster_id> must be provided.")
			}
			if err != nil {
				t.Fatalf("Failed to run script: %+v", err)
			}
			assert.GreaterOrEqual(t, len(b.String()), 0)
		})
	}
}

func TestCLIE2E_AllScriptsRepeat10(t *testing.T) {
	scripts := mustGetScriptList(t)
	repeatCount := 10
	// Run each script once.
	for _, s := range scripts {
		t.Run(s.Name, func(t *testing.T) {
			if containsStr(stressTestDisableList, s.Name) {
				t.Skip()
				return
			}
			for i := 0; i < repeatCount; i++ {
				var b *bytes.Buffer
				var err error
				switch {
				case *allClusters:
					b, err = cliExecStdoutBytes(t, "run", s.Name, "--all-clusters", "-o", "json")
				case *clusterID != "":
					b, err = cliExecStdoutBytes(t, "run", s.Name, "-c", *clusterID, "-o", "json")
				default:
					t.Fatalf("Either --all-clusters or -c <cluster_id> must be provided.")
				}
				if err != nil {
					t.Fatalf("Failed to run script: %+v", err)
				}
				assert.GreaterOrEqual(t, len(b.String()), 0)
			}
		})
	}
}

// Just repeat a simple script like agent status to make sure it works when lots of scripts are running in parallel.
func TestCLIE2E_AgentStatusStressParallel(t *testing.T) {
	t.Run("stress::px/agent_status", func(t *testing.T) {
		jobs := make(chan struct{})
		go func() {
			for i := 0; i < *stressRepeat; i++ {
				jobs <- struct{}{}
			}
			close(jobs)
		}()

		eg := errgroup.Group{}
		for i := 0; i < *stressMaxParallel; i++ {
			eg.Go(func() error {
				for range jobs {
					b, err := cliExecStdoutBytes(t, "run", "-c", *clusterID, "px/agent_status", "-o", "json")
					if err != nil {
						return err
					}
					if len(b.String()) == 0 {
						return errors.New("empty script output")
					}
				}
				return nil
			})
		}

		err := eg.Wait()
		if err != nil {
			t.Fatal(err)
		}
	})
}

// TODO(zasgar/philkuz/michelle): Fix this test since it causes a hard hang of query broker. You might have to edit
// the MaxRecvMessageSize in query_broker_server.go and run the pems for ~ 10 mins.
// func TestCLIE2E_PodStatsParallel(t *testing.T) {
// 	t.Run("stress::px/pod_stats", func(t *testing.T) {
// 		c := make(chan struct{}, 20)
// 		var wg sync.WaitGroup
// 		for i := 0; i < 100; i++ {
// 			wg.Add(1)
// 			go func() {
// 				defer wg.Done()
// 				c <- struct{}{}
// 				b, err := cliExecStdoutBytes(t, "run", "px/pod_stats", "-o", "json")
// 				if err != nil {
// 					t.Fatalf("Failed to run script: %+v", err)
// 				}
// 				assert.Greater(t, len(b.String()), 0)
// 				<-c
// 			}()
// 		}
// 		wg.Wait()
// 	})
// }

func containsStr(l []string, n string) bool {
	for _, a := range l {
		if a == n {
			return true
		}
	}
	return false
}
