package main

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"github.com/txn2/txeh"
	"golang.org/x/sync/errgroup"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"
	"k8s.io/client-go/tools/clientcmd"

	// Need this for GCP auth.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
)

var dnsEntriesByServiceCfg = map[string][]string{
	"cloud-proxy-service": {"", "work", "segment", "docs"},
	"vzconn-service":      {"cloud"},
}

var dnsEntriesByService = map[string][]string{}

type svcInfo struct {
	SvcName string
	Addr    string
}

func setupFlags() {
	pflag.String("n", "plc-dev", "The namespace to watch (plc-dev) by default")
	pflag.String("domain-name", "dev.withpixie.dev", "The domain name to use")
	pflag.String("kubeconfig", filepath.Join(homeDir(), ".kube", "config"), "(optional) absolute path to the kubeconfig file")
}

func parseFlags() {
	pflag.Parse()

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)
}

// getConfig gets the kubernetes rest config.
func getConfig() *rest.Config {
	// use the current context in kubeconfig
	config, err := clientcmd.BuildConfigFromFlags("", viper.GetString("kubeconfig"))
	if err != nil {
		log.WithError(err).Fatal("Could not build kubeconfig")
	}

	return config
}

// getClientset gets the clientset for the current kubernetes cluster.
func getClientset(config *rest.Config) *kubernetes.Clientset {
	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		log.WithError(err).Fatal("Could not create k8s clientset")
	}

	return clientset
}

func homeDir() string {
	if u := os.Getenv("SUDO_USER"); u != "" {
		return fmt.Sprintf("/home/%s", u)
	}
	if h := os.Getenv("HOME"); h != "" {
		return h
	}
	return os.Getenv("USERPROFILE") // windows
}

// watchK8sResource returns a k8s watcher for the specified resource.
func watchK8sResource(clientset *kubernetes.Clientset, resource string, namespace string) (watch.Interface, error) {
	watcher := cache.NewListWatchFromClient(clientset.CoreV1().RESTClient(), resource, namespace, fields.Everything())
	opts := metav1.ListOptions{}
	watch, err := watcher.Watch(opts)
	if err != nil {
		return nil, err
	}
	return watch, nil
}

func generateDomainEntries() {
	for svcName, domainPrefixes := range dnsEntriesByServiceCfg {
		entries := []string{}

		for _, domainPrefix := range domainPrefixes {
			entries = append(entries, getDomainEntry(domainPrefix))
		}
		dnsEntriesByService[svcName] = entries

		log.WithField("service", svcName).
			WithField("entries", strings.Join(entries, ", ")).
			Info("DNS Entries")
	}
}

func getDomainEntry(prefix string) string {
	domainSuffix := viper.GetString("domain-name")
	if len(prefix) == 0 {
		return domainSuffix
	}
	return fmt.Sprintf("%s.%s", prefix, domainSuffix)
}

func watchForExternalIP(ch <-chan watch.Event, outCh chan<- svcInfo) error {
	for u := range ch {
		svc := u.Object.(*v1.Service)
		svcName := svc.ObjectMeta.Name

		if _, ok := dnsEntriesByService[svcName]; !ok {
			continue
		}

		ing := svc.Status.LoadBalancer.Ingress
		if len(ing) > 0 && ing[0].IP != "" {
			outCh <- svcInfo{
				SvcName: svc.ObjectMeta.Name,
				Addr:    ing[0].IP,
			}
		}
	}
	return nil
}

func k8sWatchAndUpdateHosts() error {
	kubeConfig := getConfig()
	clientset := getClientset(kubeConfig)
	namespace := viper.GetString("n")
	serviceWatcher, err := watchK8sResource(clientset, "services", namespace)
	if err != nil {
		log.WithError(err).Fatal("failed to watch cloud proxy")
	}
	defer serviceWatcher.Stop()

	svcInfoCh := make(chan svcInfo)
	var g errgroup.Group
	g.Go(func() error {
		return watchForExternalIP(serviceWatcher.ResultChan(), svcInfoCh)
	})

	g.Go(func() error {
		return updateHostsFile(svcInfoCh)
	})

	return g.Wait()
}

func updateHostsFile(svcInfoCh <-chan svcInfo) error {
	for s := range svcInfoCh {
		log.WithField("service", s.SvcName).
			WithField("addr", s.Addr).
			Info("Update")
		hosts, err := txeh.NewHostsDefault()
		if err != nil {
			return err
		}

		if entries, ok := dnsEntriesByService[s.SvcName]; ok {
			hosts.RemoveHosts(entries)
			hosts.AddHosts(s.Addr, entries)
		}
		hosts.Save()
	}
	return nil
}

func cleanup() {
	log.Info("Cleaning up hosts file")
	hosts, err := txeh.NewHostsDefault()
	if err != nil {
		log.WithError(err).Fatal("Failed to get hosts file")
	}

	for _, dnsEntries := range dnsEntriesByService {
		hosts.RemoveHosts(dnsEntries)
	}
	hosts.Save()
}

func copyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()

	out, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer out.Close()

	_, err = io.Copy(out, in)
	if err != nil {
		return err
	}
	return out.Close()
}

func sudoSelfIfNotRoot() {
	uid := os.Getuid()
	if uid != 0 {
		f, _ := filepath.Abs(os.Args[0])
		args := append([]string{f}, os.Args[1:]...)

		c1 := exec.Command("sudo", args...)
		c1.Stdin = os.Stdin
		c1.Stdout = os.Stdout
		c1.Stderr = os.Stderr

		if err := c1.Run(); err != nil {
			panic(err)
		}

		err := c1.Wait()
		if err != nil {
			panic(err)
		}
		os.Exit(0)
	}
}
func main() {
	sudoSelfIfNotRoot()

	setupFlags()
	parseFlags()

	generateDomainEntries()
	err := copyFile("/etc/hosts", "/etc/hosts.bak")
	if err != nil {
		log.WithError(err).Fatal("Failed to backup /etc/hosts")
	}
	defer cleanup()

	// Also run cleanup on ctrl+c.
	c := make(chan os.Signal)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-c
		cleanup()
		os.Exit(1)
	}()

	var g errgroup.Group
	g.Go(k8sWatchAndUpdateHosts)

	// TODO(zasgar): Add Minikube tunnel

	err = g.Wait()
	if err != nil {
		log.WithError(err).Fatal("Error auto updating entries")
	}
}
