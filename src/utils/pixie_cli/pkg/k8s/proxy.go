package k8s

import (
	"context"
	"crypto/tls"
	"errors"
	"fmt"
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
	"os/exec"
	"strings"
	"time"

	log "github.com/sirupsen/logrus"
)

const (
	k8sEntityName = "svc/vizier-proxy-service"
	k8sPort       = int(443)
	k8sProxyPort  = int(31068)
	localPort     = int(31067)
)

// VizierProxy use kubectl proxy an SSL terminates to allow the UI to access
// over 127.0.0.1:31067.
type VizierProxy struct {
	ns  string
	cmd *exec.Cmd
	srv *http.Server
}

// NewVizierProxy creates a new VizierProxy.
func NewVizierProxy(ns string) *VizierProxy {
	return &VizierProxy{ns, nil, nil}
}

func waitForConnect(target string) error {
	timeout := time.After(5 * time.Second)
	insecureTransport := &http.Transport{
		TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
	}
	for {
		select {
		case <-timeout:
			return errors.New("timeout waiting for proxy")
		default:
			c := http.Client{Transport: insecureTransport}
			resp, err := c.Get(target)

			if err == nil && resp.StatusCode == http.StatusOK {
				return nil
			}
			// TODO(zasgar): Replace with spinner.
			fmt.Println("Waiting for proxy ...")
			time.Sleep(1 * time.Second)
		}
	}
}

// Run runs the proxy in a separate go routine.
func (v *VizierProxy) Run() error {
	v.cmd = exec.Command("kubectl", "-n", v.ns, "port-forward", k8sEntityName, fmt.Sprintf("%d:%d", k8sProxyPort, k8sPort))
	fmt.Println(strings.Join(v.cmd.Args, " "))
	v.cmd.Stdout = os.Stdout
	v.cmd.Stderr = os.Stderr

	err := v.cmd.Start()
	if err != nil {
		return err
	}

	proxyTarget, err := url.Parse(fmt.Sprintf("https://127.0.0.1:%d", k8sProxyPort))
	if err != nil {
		return err
	}

	insecureTransport := &http.Transport{
		TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
	}

	// Wait for the K8s proxy to come up.
	if err := waitForConnect(proxyTarget.String() + "/healthz"); err != nil {
		return err
	}
	h := httputil.NewSingleHostReverseProxy(proxyTarget)
	h.Transport = insecureTransport

	v.srv = &http.Server{
		Addr:    fmt.Sprintf("127.0.0.1:%d", localPort),
		Handler: h,
	}

	go func() {
		if err := v.srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.WithError(err).Fatal("http proxy error")
		}
	}()
	return nil
}

// Stop stops the proxy.
func (v *VizierProxy) Stop() error {
	if v.cmd != nil {
		if err := v.cmd.Process.Kill(); err != nil {
			return err
		}
	}

	_ = v.cmd.Wait()
	return v.srv.Shutdown(context.Background())
}
